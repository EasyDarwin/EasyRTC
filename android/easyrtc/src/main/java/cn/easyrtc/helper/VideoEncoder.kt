package cn.easyrtc.helper

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.HandlerThread
import android.util.Log
import cn.easyrtc.model.VideoEncodeConfig
import java.nio.ByteBuffer
import java.util.ArrayDeque
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.atomic.AtomicBoolean

class VideoEncoder(config: VideoEncodeConfig, private val colorFormat: Int) {

    private val mConfig: VideoEncodeConfig = config

    interface OnEncodedFrameListener {
        fun onEncodedFrame(data: ByteArray, offset: Int, length: Int, isKeyFrame: Boolean)
        fun onCodecConfig(data: ByteArray, length: Int)
    }

    // === 可调参数（根据设备/需求调整） ===
    private val MAX_POOL_SIZE = 6                     // 对象池最大 FrameData 数量（避免大量大对象）
    private val FRAME_QUEUE_CAPACITY = 10             // 入队上限（队列满则丢帧）
    private val TAG = "VideoEncoder"

    // 状态机
    private enum class EncoderState { CREATED, STARTING, STARTED, PAUSED, STOPPED, DESTROYED }

    @Volatile
    private var encoderState = EncoderState.CREATED

    private var mediaCodec: MediaCodec? = null
    private val isEncoding = AtomicBoolean(false)

    // 线程与 handler
    private var encoderThread: HandlerThread? = null
    private var encoderHandler: Handler? = null

    // 缓冲区复用
    private var codecConfigBuffer: ByteArray? = null
    private var codecConfigBufferSize = 0
    private var keyFrameBuffer: ByteArray? = null
    private var keyFrameBufferSize = 0
    private var outputFrameBuffer: ByteArray? = null
    private var outputFrameBufferSize = 0

    // FrameData 对象池（线程安全通过 synchronized）
    private val frameDataPool = ArrayDeque<FrameData>(MAX_POOL_SIZE)

    // 有界队列
    private val frameQueue: LinkedBlockingQueue<FrameData> = LinkedBlockingQueue(FRAME_QUEUE_CAPACITY)

    private var encodedFrameListener: OnEncodedFrameListener? = null

    data class FrameData(val buffer: ByteArray, var size: Int = 0, var timestampUs: Long = 0L) {
        fun copyFrom(src: ByteArray, length: Int, tsUs: Long) {
            // 确保长度不超过 buffer
            val copyLen = length.coerceAtMost(buffer.size)
            System.arraycopy(src, 0, buffer, 0, copyLen)
            size = copyLen
            timestampUs = tsUs
        }

        fun clear() {
            size = 0
            timestampUs = 0L
        }
    }

    // 获取或创建 FrameData（受 MAX_POOL_SIZE 限制）
    private fun obtainFrameData(requiredSize: Int): FrameData? {
        synchronized(frameDataPool) {
            // 优先复用池中可用对象，且容量足够
            val iter = frameDataPool.iterator()
            while (iter.hasNext()) {
                val fd = iter.next()
                if (fd.buffer.size >= requiredSize) {
                    iter.remove()
                    fd.clear()
                    return fd
                }
            }
            // 池中没有合适的，若池未满则创建新的，否则返回 null（告诉调用方丢帧）
            if (frameDataPool.size < MAX_POOL_SIZE) {
                try {
                    return FrameData(ByteArray(requiredSize), 0, 0L)
                } catch (oom: OutOfMemoryError) {
                    Log.e(TAG, "OOM while creating FrameData: requiredSize=$requiredSize")
                    return null
                }
            } else {
                // 池已满且没有合适的 buffer，返回 null
                return null
            }
        }
    }

    // 回收到池中（若池已满则丢弃）
    private fun recycleFrameData(fd: FrameData) {
        synchronized(frameDataPool) {
            if (frameDataPool.size < MAX_POOL_SIZE) {
                fd.clear()
                frameDataPool.addFirst(fd)
            } else {
                // 放不下就让 GC 处理
            }
        }
    }

    private fun clearFrameDataPool() {
        synchronized(frameDataPool) {
            frameDataPool.clear()
        }
    }

    // ==== 公共 API ====

    fun startEncoder(listener: OnEncodedFrameListener) {
        if (encoderState == EncoderState.STARTED || encoderState == EncoderState.STARTING) return
        this.encodedFrameListener = listener
        ensureEncoderThread()
        encoderHandler?.post {
            try {
                encoderState = EncoderState.STARTING
                initMediaCodec()
                encoderState = EncoderState.STARTED
                isEncoding.set(true)
                Log.d(TAG, "VideoEncoder started")
            } catch (e: Exception) {
                Log.e(TAG, "startEncoder failed: ${e.message}", e)
                cleanupMediaCodec()
                encoderState = EncoderState.STOPPED
            }
        }
    }

    fun pause() {
        if (encoderState != EncoderState.STARTED) return
        encoderState = EncoderState.PAUSED
        isEncoding.set(false)
        // 清空未处理帧（避免旧帧堆积）
        frameQueue.clear()
        encoderHandler?.post {
            cleanupMediaCodec()
            Log.d(TAG, "VideoEncoder paused (codec cleaned up on handler)")
        }
    }

    fun resume() {
        if (encoderState != EncoderState.PAUSED) return
        encoderState = EncoderState.STARTING
        ensureEncoderThread()
        encoderHandler?.post {
            try {
                initMediaCodec()
                encoderState = EncoderState.STARTED
                isEncoding.set(true)
                requestKeyFrameInternal()
                Log.d(TAG, "VideoEncoder resumed")
            } catch (e: Exception) {
                Log.e(TAG, "resume failed: ${e.message}", e)
                cleanupMediaCodec()
                encoderState = EncoderState.STOPPED
            }
        }
    }

    fun stop() {
        if (encoderState == EncoderState.STOPPED || encoderState == EncoderState.DESTROYED) return
        encoderState = EncoderState.STOPPED
        isEncoding.set(false)
        frameQueue.clear()
        encoderHandler?.post {
            cleanupMediaCodec()
            Log.d(TAG, "VideoEncoder stopped (codec cleaned up on handler)")
        }
    }

    fun destroy() {
        encoderState = EncoderState.DESTROYED
        isEncoding.set(false)
        frameQueue.clear()
        clearFrameDataPool()
        encoderHandler?.post {
            cleanupMediaCodec()
            encoderThread?.quitSafely()
            encoderThread = null
            encoderHandler = null
            encodedFrameListener = null
        }
        Log.d(TAG, "VideoEncoder destroyed")
    }

    fun isEncoderRunning(): Boolean = isEncoding.get() && encoderState == EncoderState.STARTED
    fun getEncoderState(): String = encoderState.name
    fun getPoolStats(): String = "Pool:${synchronizedString { frameDataPool.size }}/$MAX_POOL_SIZE Queue:${frameQueue.size}/$FRAME_QUEUE_CAPACITY"

    // ==== 内部实现 ====

    private fun ensureEncoderThread() {
        if (encoderThread == null || encoderHandler == null) {
            encoderThread = HandlerThread("VideoEncoder_Async").apply { start() }
            encoderHandler = Handler(encoderThread!!.looper)
            Log.d(TAG, "Encoder thread created")
        }
    }

    @Synchronized
    private fun initMediaCodec() {
        // 在 handler 线程上调用（外部已保证）
        cleanupMediaCodec()

        val mime = mConfig.getMimeType()
        val w = if (mConfig.getOrientation() == 90) mConfig.getHeight() else mConfig.getWidth()
        val h = if (mConfig.getOrientation() == 90) mConfig.getWidth() else mConfig.getHeight()

        val format = MediaFormat.createVideoFormat(mime, w, h).apply {
            setInteger(MediaFormat.KEY_COLOR_FORMAT, colorFormat)
            setInteger(MediaFormat.KEY_BIT_RATE, mConfig.getBitRate())
            setInteger(MediaFormat.KEY_FRAME_RATE, mConfig.getFrameRate())
            setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, mConfig.getIFrameInterval())

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) setInteger(MediaFormat.KEY_LATENCY, 0)

//            setInteger(MediaFormat.KEY_BITRATE_MODE,MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CQ)  // 质量最高
            setInteger(MediaFormat.KEY_BITRATE_MODE, MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CBR)  // 实时性最强

        }

        mediaCodec = MediaCodec.createEncoderByType(mime).apply {
            setCallback(object : MediaCodec.Callback() {
                override fun onInputBufferAvailable(codec: MediaCodec, index: Int) {
                    processInputBuffer(index)
                }

                override fun onOutputBufferAvailable(codec: MediaCodec, index: Int, info: MediaCodec.BufferInfo) {
                    processOutputBuffer(index, info)
                }

                override fun onOutputFormatChanged(codec: MediaCodec, format: MediaFormat) {
                    // Output format may contain csd-0/csd-1
                    Log.d(TAG, "Output format changed: $format")
                    // 如果想提取 csd-0/csd-1，可以在这里处理（但 current impl 在 BUFFER_FLAG_CODEC_CONFIG 时处理）
                }

                override fun onError(codec: MediaCodec, e: MediaCodec.CodecException) {
                    Log.e(TAG, "MediaCodec error: ${e.message}", e)
                    recoverMediaCodec()
                }
            }, encoderHandler)

            configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
            start()
            Log.d(TAG, "MediaCodec configured and started")
        }
    }

    private fun processInputBuffer(index: Int) {
        if (!isEncoding.get()) return
        val codec = mediaCodec ?: return

        var fd: FrameData? = null
        try {
            fd = frameQueue.poll() // 非阻塞取帧
            val inputBuffer: ByteBuffer? = try {
                codec.getInputBuffer(index)
            } catch (e: Exception) {
                null
            }
            if (fd != null && inputBuffer != null && fd.size > 0) {
                inputBuffer.clear()
                inputBuffer.put(fd.buffer, 0, fd.size)
                try {
                    codec.queueInputBuffer(index, 0, fd.size, fd.timestampUs, 0)
                } catch (e: Exception) {
                    Log.e(TAG, "queueInputBuffer failed: ${e.message}", e)
                } finally {
                    recycleFrameData(fd)
                    fd = null
                }
            } else {
                // 没有帧可编码，提交空 buffer（非关键）
                try {
                    if (isEncoding.get()) codec.queueInputBuffer(index, 0, 0, 0, 0)
                } catch (e: Exception) {
                    // ignore
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "processInputBuffer error: ${e.message}", e)
            if (fd != null) recycleFrameData(fd)
        }
    }

    private fun processOutputBuffer(index: Int, info: MediaCodec.BufferInfo) {
        val codec = mediaCodec ?: return
        try {
            if (info.size <= 0) {
                codec.releaseOutputBuffer(index, false)
                return
            }
            val outBuffer = try {
                codec.getOutputBuffer(index)
            } catch (e: Exception) {
                null
            } ?: run {
                codec.releaseOutputBuffer(index, false)
                return
            }

            ensureOutputBufferCapacity(info.size)
            // outBuffer 的 offset/limit 可能非 0
            outBuffer.position(info.offset)
            outBuffer.limit(info.offset + info.size)

            outBuffer.get(outputFrameBuffer!!, 0, info.size)
//          Log.d(TAG, "processOutputBuffer  key = " +  (info.flags and MediaCodec.BUFFER_FLAG_KEY_FRAME != 0))
            when {
                // 处理 sps+pps
                info.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG != 0 -> {
                    ensureCodecConfigBufferCapacity(info.size)
                    System.arraycopy(outputFrameBuffer!!, 0, codecConfigBuffer!!, 0, info.size)
                    encodedFrameListener?.onCodecConfig(outputFrameBuffer!!, info.size)
                }

                info.flags and MediaCodec.BUFFER_FLAG_KEY_FRAME != 0 -> {
                    processKeyFrame(outputFrameBuffer!!, info)
                }

                else -> {
                    encodedFrameListener?.onEncodedFrame(outputFrameBuffer!!, 0, info.size, false)
                }
            }

            codec.releaseOutputBuffer(index, false)
        } catch (e: Exception) {
            Log.e(TAG, "processOutputBuffer error: ${e.message}", e)
            try {
                codec.releaseOutputBuffer(index, false)
            } catch (_: Exception) {
            }
        }
    }

    private fun processKeyFrame(frameData: ByteArray, info: MediaCodec.BufferInfo) {
        if (codecConfigBuffer == null || codecConfigBufferSize == 0) {
            encodedFrameListener?.onEncodedFrame(frameData, 0, info.size, true)
            return
        }

//        Log.d(TAG, " processKeyFrame  codecConfigBufferSize=$codecConfigBufferSize,  info.size=${info.size}")

        val required = codecConfigBufferSize + info.size

        ensureKeyFrameBufferCapacity(required)

        System.arraycopy(codecConfigBuffer!!, 0, keyFrameBuffer!!, 0, codecConfigBufferSize)
        System.arraycopy(frameData, 0, keyFrameBuffer!!, codecConfigBufferSize, info.size)

        encodedFrameListener?.onEncodedFrame(keyFrameBuffer!!, 0, required, true)

//        MagicFileHelper.getInstance().saveToFile(keyFrameBuffer!!,"${System.currentTimeMillis()}.h264",false)
    }

    fun sendFrameToMediaCodec(data: ByteArray, size: Int = data.size, timestampUs: Long = System.nanoTime() / 1000) {
        if (!isEncoding.get()) return

        // 计算需要的 buffer 大小（传入 size）
        val required = size
        val fd = obtainFrameData(required)
        if (fd == null) {
            // 对象池已满或内存受限，丢帧（避免 OOM）
            Log.w(TAG, "No FrameData available (pool full or OOM), dropping frame")
            return
        }
        fd.copyFrom(data, size, timestampUs)

        if (!frameQueue.offer(fd)) {
            // 入队失败，回收并丢帧
            recycleFrameData(fd)
            Log.w(TAG, "Frame queue full, dropping frame. queue=${frameQueue.size}")
        }
    }

    fun sendFramesToMediaCodec(frames: List<ByteArray>) {
        if (!isEncoding.get() || frames.isEmpty()) return
        frames.forEach { frame ->
            sendFrameToMediaCodec(frame, frame.size)
        }
    }

    fun requestKeyFrame() {
        // 直接在调用线程设置参数（安全做法：可以在 handler 线程调用）
        try {
            mediaCodec?.setParameters(Bundle().apply { putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0) })
            Log.d(TAG, "requestKeyFrame called")
        } catch (e: Exception) {
            Log.e(TAG, "requestKeyFrame failed: ${e.message}", e)
        }
    }

    private fun requestKeyFrameInternal() {
        requestKeyFrame()
    }

    @Synchronized
    private fun cleanupMediaCodec() {
        mediaCodec?.let {
            try {
                it.stop()
            } catch (e: Exception) { /* ignore */
            }
            try {
                it.release()
            } catch (e: Exception) { /* ignore */
            }
            mediaCodec = null
        }
        // 不清 codecConfigBuffer/ keyFrameBuffer / outputFrameBuffer（复用），只在必要时重新分配
        Log.d(TAG, "MediaCodec cleaned up")
    }

    private fun recoverMediaCodec() {
        encoderHandler?.post {
            cleanupMediaCodec()
            if (encoderState == EncoderState.STARTED) {
                try {
                    initMediaCodec()
                } catch (e: Exception) {
                    Log.e(TAG, "recoverMediaCodec init failed: ${e.message}")
                }
            }
        }
    }

    // 缓冲区确保函数
    private fun ensureOutputBufferCapacity(requiredSize: Int) {
        if (outputFrameBuffer == null || outputFrameBufferSize < requiredSize) {
            outputFrameBuffer = ByteArray(requiredSize)
            outputFrameBufferSize = requiredSize
            Log.d(TAG, "Output buffer resized: $outputFrameBufferSize $requiredSize")
        }
    }

    private fun ensureCodecConfigBufferCapacity(requiredSize: Int) {
        if (codecConfigBuffer == null || codecConfigBufferSize < requiredSize) {
            codecConfigBuffer = ByteArray(requiredSize)
            codecConfigBufferSize = requiredSize
            Log.d(TAG, "Codec config buffer resized: $requiredSize")
        }
    }

    private fun ensureKeyFrameBufferCapacity(requiredSize: Int) {
        if (keyFrameBuffer == null || keyFrameBufferSize < requiredSize) {
            keyFrameBuffer = ByteArray(requiredSize)
            keyFrameBufferSize = requiredSize
            Log.d(TAG, "Key frame buffer resized: $requiredSize")
        }
    }

    // === 工具 ===
    private inline fun <T> synchronizedString(block: () -> T): T {
        synchronized(frameDataPool) { return block() }
    }
}
