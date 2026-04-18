package cn.easyrtc.media

import android.view.Surface
import cn.easyrtc.model.VideoEncodeConfig

class MediaPipeline(
    transceiverHandle: Long,
    config: VideoEncodeConfig
) {

    private var nativePtr: Long = nativeCreate(
        transceiverHandle,
        if (config.getUseHevc()) CODEC_H265 else CODEC_H264,
        config.getWidth(),
        config.getHeight(),
        config.getBitRate(),
        config.getFrameRate(),
        config.getIFrameInterval()
    )

    fun setSurface(): Surface? {
        return nativeSetSurface(nativePtr)
    }

    fun start(): Int {
        return nativeStart(nativePtr)
    }

    fun stop() {
        nativeStop(nativePtr)
    }

    fun release() {
        if (nativePtr != 0L) {
            nativeRelease(nativePtr)
            nativePtr = 0L
        }
    }

    fun requestKeyFrame() {
        nativeRequestKeyFrame(nativePtr)
    }

    fun setPreviewSurface(surface: Surface) { nativeSetPreviewSurface(nativePtr, surface) }
    fun switchCamera() { nativeSwitchCamera(nativePtr) }
    fun setTransceiver(handle: Long) { nativeSetTransceiver(nativePtr, handle) }

    private external fun nativeCreate(
        transceiverHandle: Long,
        codec: Int,
        width: Int,
        height: Int,
        bitrate: Int,
        fps: Int,
        iframeInterval: Int
    ): Long

    private external fun nativeSetSurface(pipelinePtr: Long): Surface?
    private external fun nativeStart(pipelinePtr: Long): Int
    private external fun nativeStop(pipelinePtr: Long)
    private external fun nativeRelease(pipelinePtr: Long)
    private external fun nativeRequestKeyFrame(pipelinePtr: Long)
    private external fun nativeSetPreviewSurface(pipelinePtr: Long, surface: Surface)
    private external fun nativeSwitchCamera(pipelinePtr: Long)
    private external fun nativeSetTransceiver(pipelinePtr: Long, handle: Long)

    companion object {
        const val CODEC_H264 = 1
        const val CODEC_H265 = 6
        const val FACING_BACK = 0
        const val FACING_FRONT = 1

        init {
            System.loadLibrary("easyrtc_media")
        }
    }
}
