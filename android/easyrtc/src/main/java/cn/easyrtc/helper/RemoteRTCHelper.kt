package cn.easyrtc.helper

import android.graphics.PorterDuff
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.util.Log
import android.view.Surface
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong

class RemoteRTCHelper(
    private val surface: Surface,
    private val codecType: String = MediaFormat.MIMETYPE_VIDEO_AVC,
    private val videoWidth: Int = 1920,
    private val videoHeight: Int = 1080,
    private val frameRate: Int = 30,
    private val queueCapacity: Int = 30,
    private val iFrameInterval: Int = 1,
) {
    // 视频解码器相关
    private var videoDecoder: MediaCodec? = null
    private var videoDecoderThread: HandlerThread? = null
    private var videoDecoderHandler: Handler? = null

    // 视频帧队列管理
    private val videoFrameQueue = LinkedBlockingQueue<ByteArray>(queueCapacity)

    // 使用原子操作保证线程安全
    private val isDecoderRunning = AtomicBoolean(false)
    private val isConsumerRunning = AtomicBoolean(false)
    private val isDestroyed = AtomicBoolean(false)
    private val decoderLock = Any()

    // 当前编码类型
    private var currentCodecType: String = codecType // 默认264

    // 性能统计
    private val framesProcessed = AtomicLong(0)
    private val decoderErrorCount = AtomicLong(0)

    // 常量
    companion object {
        private const val TAG = "RemoteRTCHelper"
        private const val MAX_DECODER_ERROR_COUNT = 5
        private const val DEQUEUE_TIMEOUT_US: Long = 10000

        // 编码类型常量
        const val CODEC_H264 = MediaFormat.MIMETYPE_VIDEO_AVC
        const val CODEC_H265 = MediaFormat.MIMETYPE_VIDEO_HEVC
    }

    // 修复：将 videoFrameConsumer 改为 lazy 初始化或方法
    private fun getVideoFrameConsumer(): Runnable {
        return Runnable {
            val bufferInfo = MediaCodec.BufferInfo()
            Log.d(TAG, "Video frame consumer thread started successfully")

            try {
                while (isDecoderRunning.get() && !isDestroyed.get()) {
                    try {
                        val frameData = videoFrameQueue.take()
                        processVideoFrame(frameData, bufferInfo)
                    } catch (e: InterruptedException) {
                        Log.d(TAG, "Video frame consumer interrupted")
                        break
                    } catch (e: Exception) {
                        Log.e(TAG, "Video frame consumer error: ${e.message}")
                        e.printStackTrace()
                        // 防止异常导致循环崩溃
                        Thread.sleep(10)
                    }
                }
            } finally {
                isConsumerRunning.set(false)
                Log.d(TAG, "Video frame consumer thread exited")
            }
        }
    }

    init {
        initVideoDecoder()
    }

    // 初始化视频解码器
    fun initVideoDecoder(codecType: String? = null) {
        if (isDestroyed.get()) {
            Log.w(TAG, "Attempt to init decoder after destruction")
            return
        }

        synchronized(decoderLock) {
            // 如果指定了新的编码类型，则更新
            codecType?.let {
                if (it != currentCodecType) {
                    Log.d(TAG, "Switching codec from $currentCodecType to $it")
                    currentCodecType = it
                    // 先释放旧的解码器
                    releaseVideoDecoderOnly()
                }
            }

            // 如果解码器已经在运行，则不需要重新初始化
            if (isDecoderRunning.get() && videoDecoder != null) {
                Log.d(TAG, "Decoder already running, skip reinitialization")
                return
            }

            try {
                // 创建并配置解码器
                videoDecoder = MediaCodec.createDecoderByType(currentCodecType)
                val format = MediaFormat.createVideoFormat(currentCodecType, 720, 1280)
                format.setInteger(MediaFormat.KEY_FRAME_RATE, frameRate)
                format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, iFrameInterval)
                format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface)

                videoDecoder?.configure(format, surface, null, 0)
                videoDecoder?.start()

                // 启动解码线程
                videoDecoderThread = HandlerThread("VideoDecoder").apply { start() }
                videoDecoderHandler = Handler(videoDecoderThread!!.looper)

                isDecoderRunning.set(true)
                decoderErrorCount.set(0)

                // 启动帧消费线程
                startFrameConsumer()

                Log.d(TAG, "Video decoder initialized successfully for $currentCodecType")
            } catch (e: Exception) {
                Log.e(TAG, "Video decoder init error: ${e.message}")
                e.printStackTrace()
                // 初始化失败时清理资源
                releaseVideoDecoderOnly()
            }
        }
    }

    // 启动帧消费线程
    private fun startFrameConsumer() {
        if (isConsumerRunning.get()) {
            Log.d(TAG, "Frame consumer already running")
            return
        }

        videoDecoderHandler?.post {
            if (isDecoderRunning.get() && !isDestroyed.get() && !isConsumerRunning.get()) {
                isConsumerRunning.set(true)
                Log.d(TAG, "Starting video frame consumer")
                // 修复：使用方法获取 Runnable 实例
                getVideoFrameConsumer().run()
            } else {
                Log.w(
                    TAG,
                    "Video decoder not in running state, skipping frame consumer. " + "isDecoderRunning: ${isDecoderRunning.get()}, " + "isDestroyed: ${isDestroyed.get()}, " + "isConsumerRunning: ${isConsumerRunning.get()}"
                )
            }
        } ?: run {
            Log.e(TAG, "Video decoder handler is null, cannot start frame consumer")
        }
    }

    // 重新初始化解码器（用于切换编码格式）
    fun reinitVideoDecoder(codecType: String) {
        Log.d(TAG, "reinitVideoDecoder decoder from $currentCodecType to $codecType")
        if (isDestroyed.get()) return

        synchronized(decoderLock) {
            if (codecType != currentCodecType) {
                Log.d(TAG, "Reinitializing decoder from $currentCodecType to $codecType")

                // 停止当前解码器
                isDecoderRunning.set(false)
                isConsumerRunning.set(false)
                videoFrameQueue.clear()

                // 释放解码器资源
                releaseVideoDecoderOnly()

                // 更新编码类型
                currentCodecType = codecType

                // 重新初始化
                initVideoDecoder()
            }
        }
    }

    // 仅释放解码器资源，不停止整个组件
    private fun releaseVideoDecoderOnly() {
        synchronized(decoderLock) {
            isDecoderRunning.set(false)
            isConsumerRunning.set(false)

            videoDecoderHandler?.removeCallbacksAndMessages(null)
            videoDecoderThread?.quitSafely()

            try {
                videoDecoder?.stop()
                videoDecoder?.release()
            } catch (e: Exception) {
                Log.e(TAG, "Video decoder release error: ${e.message}")
            }

            videoDecoder = null
            videoDecoderThread = null
            videoDecoderHandler = null

            Log.d(TAG, "Video decoder released")
        }
    }

    // 视频帧处理逻辑封装
    private fun processVideoFrame(data: ByteArray, bufferInfo: MediaCodec.BufferInfo) {
        val decoder = videoDecoder ?: run {
            Log.w(TAG, "Video decoder is null in processVideoFrame")
            return
        }

        try {
            // 处理输入缓冲区
            val inputBufferId = decoder.dequeueInputBuffer(DEQUEUE_TIMEOUT_US)
            if (inputBufferId >= 0) {
                decoder.getInputBuffer(inputBufferId)?.apply {
                    clear()
                    put(data)
                }
                decoder.queueInputBuffer(
                    inputBufferId, 0, data.size, System.nanoTime() / 1000, 0
                )
                framesProcessed.incrementAndGet()

                // 调试日志（每处理100帧打印一次）
                if (framesProcessed.get() % 100 == 0L) {
                    Log.d(TAG, "Processed ${framesProcessed.get()} frames, queue size: ${videoFrameQueue.size}")
                }
            } else {
                when (inputBufferId) {
                    MediaCodec.INFO_TRY_AGAIN_LATER -> {
                        Log.w(TAG, "No available input buffer for video frame (try again later)")
                    }

                    else -> {
                        Log.w(TAG, "No available input buffer for video frame, code: $inputBufferId")
                    }
                }
                return
            }

            // 处理输出缓冲区
            while (isDecoderRunning.get() && !isDestroyed.get()) {
                val outputBufferId = decoder.dequeueOutputBuffer(bufferInfo, DEQUEUE_TIMEOUT_US)
                when {
                    outputBufferId >= 0 -> {
                        // 释放输出缓冲区并渲染
                        decoder.releaseOutputBuffer(outputBufferId, true)
                        break // 处理一帧后退出，避免阻塞
                    }

                    outputBufferId == MediaCodec.INFO_TRY_AGAIN_LATER -> break
                    outputBufferId == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> {
                        Log.d(TAG, "Decoder output format changed")
                    }

                    outputBufferId == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED -> {
                        Log.d(TAG, "Decoder output buffers changed")
                    }
                }
            }

            decoderErrorCount.set(0) // 成功处理帧，重置错误计数

        } catch (e: Exception) {
            Log.e(TAG, "Video frame processing error: ${e.message}")
            e.printStackTrace()
            val errorCount = decoderErrorCount.incrementAndGet()

            // 如果连续出错，尝试重新初始化解码器
            if (errorCount >= MAX_DECODER_ERROR_COUNT) {
                Log.w(TAG, "Too many decoder errors ($errorCount), attempting to reinitialize...")
                Handler(Looper.getMainLooper()).post {
                    if (!isDestroyed.get()) {
                        reinitVideoDecoder(currentCodecType)
                    }
                }
            }
        }
    }

    // 处理远程视频帧
    fun onRemoteVideoFrame(data: ByteArray) {
        if (!isDecoderRunning.get() || isDestroyed.get()) {
            Log.w(TAG, "Decoder not running or destroyed, frame dropped")
            return
        }

        // 检查消费线程是否在运行
        if (!isConsumerRunning.get()) {
            Log.w(TAG, "Frame consumer not running, attempting to restart...")
            startFrameConsumer()
        }

        // 非阻塞式添加，队列满时丢弃旧帧
        if (!videoFrameQueue.offer(data)) {
            // 队列已满，丢弃最旧帧并记录统计
            if (videoFrameQueue.isNotEmpty()) {
                videoFrameQueue.poll() // 移除队首旧帧
                Log.w(TAG, "Video frame queue full, replaced oldest frame")
            }
            // 再次尝试添加新帧
            if (!videoFrameQueue.offer(data)) {
                Log.w(TAG, "Failed to add video frame to queue, frame dropped")
            }
        }

        // 调试日志（每100帧打印一次）
        if (framesProcessed.get() % 100 == 0L) {
            Log.v(TAG, "Video frame queued. Size: ${data.size}, Queue: ${videoFrameQueue.size}")
        }
    }

    // 释放视频解码器资源
    fun releaseVideoHandler() {
        if (isDestroyed.get()) return

        synchronized(decoderLock) {
            isDecoderRunning.set(false)
            isConsumerRunning.set(false)
            isDestroyed.set(true)

            // 清空队列
            videoFrameQueue.clear()

            videoDecoderHandler?.removeCallbacksAndMessages(null)
            videoDecoderThread?.quitSafely()

            try {
                videoDecoder?.stop()
                videoDecoder?.release()
            } catch (e: Exception) {
                Log.e(TAG, "Video decoder release error: ${e.message}")
            }

            videoDecoder = null
            videoDecoderThread = null
            videoDecoderHandler = null

            Log.d(TAG, "RemoteRTCHelper released")
        }
    }

    fun release() {
        releaseVideoHandler()
    }

    private fun clearSurface(surface: Surface?) {
        if (surface == null) return
        try {
            val canvas = surface.lockCanvas(null)
            canvas.drawColor(0, PorterDuff.Mode.CLEAR)
            surface.unlockCanvasAndPost(canvas)
            Log.d(TAG, "Surface cleared with black frame")
        } catch (e: Exception) {
            Log.e(TAG, "clearSurface error: ${e.message}")
        }
    }
}
