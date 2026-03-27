package cn.easyrtc.helper

import cn.easyrtc.model.VideoEncodeConfig
import android.content.Context
import android.graphics.ImageFormat
import android.graphics.SurfaceTexture
import android.hardware.Camera
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.util.Log
import org.easydarwin.sw.JNIUtil
import java.util.concurrent.ArrayBlockingQueue
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean

class CameraHelper(private val context: Context, private val videoEncodeConfig: VideoEncodeConfig) {
    private var camera: Camera? = null
    private val mVideoEncodeConfig = videoEncodeConfig

    private var displayOrientation: Int = mVideoEncodeConfig.getOrientation()
    private var previewWidth: Int = mVideoEncodeConfig.getWidth()
    private var previewHeight: Int = mVideoEncodeConfig.getHeight()

    private var encodedWidth: Int = mVideoEncodeConfig.getWidth()
    private var encodedHeight: Int = mVideoEncodeConfig.getHeight()
    private var encodedRotation: Int = mVideoEncodeConfig.getOrientation()

    private var mCameraID: Int = mVideoEncodeConfig.getCameraId()

    // 预览回调相关变量
    private val callbackBuffers = mutableListOf<ByteArray>()
    private val BUFFER_COUNT = 3

    private val useHevc = mVideoEncodeConfig.getUseHevc()

    var info: MediaHelper.CodecInfo = MediaHelper.CodecInfo()

    var mVideoEncoder: VideoEncoder? = null

    // 相机状态监听器
    interface CameraListener {
        fun onCameraOpened(camera: Camera)
        fun onCameraError(error: String)
        fun sendVideoFrame(data: ByteArray, length: Int, frameType: Int, pts: Long)
    }

    private var i420_buffer: ByteArray? = null

    // 用于存放供编码使用的可复用缓冲池（避免频繁分配）
    private lateinit var encodeBufferPool: ArrayBlockingQueue<ByteArray>

    // 队列用于编码线程消费，容量可调
    private val frameQueue: LinkedBlockingQueue<ByteArray> = LinkedBlockingQueue(20)

    // 编码工作线程控制
    private var encoderThread: Thread? = null
    private val encoderRunning = AtomicBoolean(false)

    // 打开相机
    fun openCamera(surfaceTexture: SurfaceTexture?, listener: CameraListener) {

        if (surfaceTexture == null) {
            listener.onCameraError("SurfaceTexture is null")
            return
        }

        try {
            // 释放可能存在的旧相机实例
            releaseCamera()

            getEncodedDimensions() //获取设备 宽高以及旋转角度
            // 打开指定相机
            camera = Camera.open(mCameraID).apply {
                // 设置相机参数
                val params = parameters.apply {
                    // 查找并设置最佳预览尺寸
                    supportedPreviewSizes.find {
                        it.width == previewWidth && it.height == previewHeight
                    }?.let {
                        setPreviewSize(it.width, it.height)
                        Log.d(TAG, "Set preview size: ${it.width}x${it.height}")
                    } ?: run {
                        // 如果没有找到精确匹配，使用第一个可用尺寸
                        supportedPreviewSizes.firstOrNull()?.let {
                            setPreviewSize(it.width, it.height)
                            previewWidth = it.width
                            previewHeight = it.height
                            Log.w(TAG, "Using fallback preview size: ${it.width}x${it.height}")
                        }
                    }

                    // 自动对焦设置（保持原样）
                    val focusModes = supportedFocusModes
                    when {
                        focusModes.contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO) -> {
                            focusMode = Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO
                            Log.d(TAG, "启用连续视频自动对焦")
                        }

                        focusModes.contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE) -> {
                            focusMode = Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE
                            Log.d(TAG, "启用连续图片自动对焦")
                        }

                        focusModes.contains(Camera.Parameters.FOCUS_MODE_AUTO) -> {
                            focusMode = Camera.Parameters.FOCUS_MODE_AUTO
                            Log.d(TAG, "启用自动对焦")
                        }

                        else -> {
                            Log.w(TAG, "自动对焦不可用，使用默认对焦模式: $focusModes")
                        }
                    }

                    // 设置预览格式（默认为NV21）
                    previewFormat = ImageFormat.NV21
                }

                // 应用参数设置
                parameters = params

                // 设置显示方向
                setDisplayOrientation(displayOrientation)

                // 设置预览SurfaceTexture
                setPreviewTexture(surfaceTexture)

                // 开始预览
                startPreview()
                Log.d(TAG, "Camera preview started")
                // 通知监听器相机已打开
                listener.onCameraOpened(this)
            }
            // 初始化预览回调缓冲区
            setupPreviewCallback()
            // 初始化编码器（如果不存在）
            if (mVideoEncoder == null) {
                initVideoEncoder(listener)
            } else {
                // 如果编码器已存在，恢复编码
                resumeEncoding()
            }

            // 启动编码工作线程（在 encoder 初始化之后启动）
            startEncoderThread()

        } catch (e: Exception) {
            val errorMsg = "Camera setup error: ${e.message}"
            Log.e(TAG, errorMsg)
            listener.onCameraError(errorMsg)
        }
    }


    // 在初始化编码器的地方替换为：
    private fun initVideoEncoder(listener: CameraListener) {
        mVideoEncoder = VideoEncoder(mVideoEncodeConfig, info.mColorFormat)

        mVideoEncoder?.startEncoder(object : VideoEncoder.OnEncodedFrameListener {
            override fun onEncodedFrame(data: ByteArray, offset: Int, length: Int, isKeyFrame: Boolean) {
                listener.sendVideoFrame(data, length, if (isKeyFrame) 1 else 0, System.currentTimeMillis())
            }

            override fun onCodecConfig(data: ByteArray, length: Int) {
                Log.d(TAG, "Received codec config data, size: $length")
            }
        })
    }

    // 启动编码工作线程：从 frameQueue 拉取 buffer 发送给 encoder，并把 buffer 回收回池
    private fun startEncoderThread() {
        if (encoderRunning.get()) return
        encoderRunning.set(true)
        encoderThread = Thread {
            Log.d(TAG, "Encoder thread started")
            while (encoderRunning.get()) {
                try {
                    // 等待一个待编码缓冲，超时循环检查 running 状态
                    val buf = frameQueue.poll(200, TimeUnit.MILLISECONDS) ?: continue
                    try {
                        // 这里直接调用 encoder（假设不会让 encoder 长时间持有该数组引用）
                        mVideoEncoder?.sendFrameToMediaCodec(buf)
                    } catch (e: Exception) {
                        Log.e(TAG, "Error sending to encoder: ${e.message}")
                    } finally {
                        // 尝试把 buf 归还到池（若池满则丢弃）
                        val offered = encodeBufferPool.offer(buf)
                        if (!offered) {
                            // 池满说明回收路径有问题，丢弃并让 gc 回收
                            Log.w(TAG, "encodeBufferPool is full, dropping buffer")
                        }
                    }
                } catch (e: InterruptedException) {
                    // 线程被中断，退出循环
                    Log.w(TAG, "Encoder thread interrupted")
                    break
                } catch (e: Exception) {
                    Log.e(TAG, "Encoder thread error: ${e.message}")
                }
            }
            Log.d(TAG, "Encoder thread exiting")
        }.also { it.isDaemon = true; it.start() }
    }

    private fun stopEncoderThread() {
        encoderRunning.set(false)
        encoderThread?.interrupt()
        encoderThread = null
    }


    // 初始化预览回调缓冲区（并创建 encodeBufferPool）
    private fun setupPreviewCallback() {
        camera?.let { cam ->
            // 移除旧回调
            cam.setPreviewCallbackWithBuffer(null)
            callbackBuffers.clear()

            // 计算帧缓冲区大小
            val format = cam.parameters.previewFormat
            val bitsPerPixel = when (format) {
                ImageFormat.NV21 -> 12  // NV21每像素12位 (YUV420SP)
                ImageFormat.YV12 -> 12  // YV12每像素12位 (YUV420P)
                else -> ImageFormat.getBitsPerPixel(format)
            }
            val bufferSize = previewWidth * previewHeight * bitsPerPixel / 8

            // 初始化 encodeBufferPool（池容量可比 frameQueue 稍大）
            val poolSize = (frameQueue.remainingCapacity() + 10).coerceAtLeast(BUFFER_COUNT + 2)
            encodeBufferPool = ArrayBlockingQueue(poolSize)
            // 预先填充一些池
            repeat(poolSize) {
                encodeBufferPool.offer(ByteArray(bufferSize))
            }

            // 确保 i420_buffer 有合适大小（复用）
            if (i420_buffer == null || i420_buffer!!.size < bufferSize) {
                i420_buffer = ByteArray(bufferSize)
            }

            // 分配 camera 回调缓冲区（相机专用）
            repeat(BUFFER_COUNT) {
                val buffer = ByteArray(bufferSize)
                callbackBuffers.add(buffer)
                cam.addCallbackBuffer(buffer)
            }

            // 设置带缓冲区的回调
            cam.setPreviewCallbackWithBuffer { data, camera ->
                if (data == null) return@setPreviewCallbackWithBuffer

                try {
                    // 转 I420（旋转等） -> i420_buffer （复用）
                    // JNIUtil.ConvertToI420(sourceNV21, dstI420, srcW, srcH, sx, sy, sw, sh, rotation, something)
                    JNIUtil.ConvertToI420(data, i420_buffer, previewWidth, previewHeight, 0, 0, previewWidth, previewHeight, encodedRotation % 360, 2)

                    // 从池中取 encodeBuf（如果池空则丢帧以避免分配）
                    val encodeBuf = encodeBufferPool.poll() // 不会新建，null 表示暂时没有可用 buffer
                    if (encodeBuf == null) {
                        // 池空：选择丢帧（记录日志），并立即归还 camera 的回调缓冲区
                        Log.w(TAG, "encodeBufferPool empty - drop frame to avoid allocation/GC")
                        camera.addCallbackBuffer(data)
                        return@setPreviewCallbackWithBuffer
                    }

                    // 将 i420 转成编码器需要的格式到 encodeBuf（注意：两次拷贝—可改进）
                    when (info.mColorFormat) {
                        MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar, MediaCodecInfo.CodecCapabilities.COLOR_TI_FormatYUV420PackedSemiPlanar -> {
                            JNIUtil.ConvertFromI420(i420_buffer, encodeBuf, encodedWidth, encodedHeight, 3)
                        }

                        MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Planar -> {
                            JNIUtil.ConvertFromI420(i420_buffer, encodeBuf, encodedWidth, encodedHeight, 0)
                        }

                        else -> {
                            // 兜底：尽量做一次转换（注意参数）
                            JNIUtil.ConvertFromI420(i420_buffer, encodeBuf, encodedWidth, encodedWidth, 0)
                        }
                    }

                    // 把用于编码的缓冲加入编码队列，由编码线程消费
                    val offered = frameQueue.offer(encodeBuf)
                    if (!offered) {
                        // 队列满：把缓冲回收到池中并丢帧
                        Log.w(TAG, "frameQueue full - drop frame")
                        encodeBufferPool.offer(encodeBuf)
                    }

                } catch (e: Exception) {
                    Log.e(TAG, "Preview callback processing error: ${e.message}")
                    // 若发生异常，尽量把 camera buffer 归还
                } finally {
                    // 重要：原始 data 必须立即归还给 camera（避免复用冲突）
                    try {
                        camera.addCallbackBuffer(data)
                    } catch (ex: Exception) {
                        Log.e(TAG, "Failed to add callback buffer back: ${ex.message}")
                    }
                }
            }
        }
    }

    // 暂停编码（保持MediaCodec实例）
    private fun pauseEncoding() {
        mVideoEncoder?.pause()
        // 暂停消费：停止编码线程
        stopEncoderThread()
    }

    // 恢复编码
    private fun resumeEncoding() {
        mVideoEncoder?.resume()
        // 恢复消费：重启编码线程
        startEncoderThread()
    }


    // 释放相机资源（不释放编码器）
    fun releaseCamera() {
        // 先暂停编码
        pauseEncoding()

        // 停止预览和回调
        camera?.apply {
            try {
                setPreviewCallbackWithBuffer(null)
                stopPreview()
                Log.d(TAG, "Camera preview stopped")
            } catch (e: Exception) {
                Log.e(TAG, "Error stopping camera preview: ${e.message}")
            }
        }

        // 清空回调缓冲区
        callbackBuffers.clear()

        // 清空帧队列
        frameQueue.clear()

        // 停止编码线程（如果还在）
        stopEncoderThread()

        // 最后释放相机实例
        camera?.apply {
            try {
                release()
                Log.d(TAG, "Camera released")
            } catch (e: Exception) {
                Log.e(TAG, "Error releasing camera: ${e.message}")
            }
            camera = null
        }
    }

    // 切换相机
    fun switchCamera(surfaceTexture: SurfaceTexture?, listener: CameraListener) {
        // 暂停编码而不是完全停止
        pauseEncoding()

        mCameraID = if (mCameraID == Camera.CameraInfo.CAMERA_FACING_BACK) {
            Camera.CameraInfo.CAMERA_FACING_FRONT
        } else {
            Camera.CameraInfo.CAMERA_FACING_BACK
        }

        openCamera(surfaceTexture, listener)
    }

    // 改进的释放方法
    fun release() {
        Log.d(TAG, "Releasing CameraHelper resources")

        // 先释放相机
        releaseCamera()
        // 然后停止编码
        mVideoEncoder?.let {
            try {
                it.stop() // 假设 VideoEncoder 有 stop 方法；若无则调用适当释放
            } catch (e: Exception) {
                Log.e(TAG, "Error stopping encoder: ${e.message}")
            }
        }
        mVideoEncoder = null

        Log.d(TAG, "CameraHelper fully released")
    }

    fun getEncodedDimensions() {

        val infos: ArrayList<MediaHelper.CodecInfo> = MediaHelper.getListEncoders(if (useHevc) MediaFormat.MIMETYPE_VIDEO_HEVC else MediaFormat.MIMETYPE_VIDEO_AVC)

        for (info in infos) {
            Log.d(TAG, "CodecInfo: name=${info.mName}, colorFormat=${info.mColorFormat}")
        }

        if (!infos.isEmpty()) {
            val ci: MediaHelper.CodecInfo = infos.get(0)
            info.mName = ci.mName
            info.mColorFormat = ci.mColorFormat
        } else {
            // 注意：Toast 必须在主线程显示；这里用 Log，同时尽量避免频繁弹长提示
            Log.e(TAG, "不支持 ${if (useHevc) 265 else 264} 硬件编码;请更换 硬件编码器")
        }

        encodedRotation = if (mCameraID == Camera.CameraInfo.CAMERA_FACING_FRONT) (360 - displayOrientation) % 360
        else displayOrientation

        encodedWidth = if (encodedRotation % 360 == 90 || encodedRotation % 360 == 270) previewHeight
        else previewWidth

        encodedHeight = if (encodedRotation % 360 == 90 || encodedRotation % 360 == 270) previewWidth
        else previewHeight
    }


    companion object {
        private const val TAG = "CameraHelper"
    }
}