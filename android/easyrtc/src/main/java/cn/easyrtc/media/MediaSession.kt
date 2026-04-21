package cn.easyrtc.media

import android.view.Surface
import cn.easyrtc.model.VideoEncodeConfig

class MediaSession {

    private var nativePtr: Long = 0

    fun create() {
        assert(nativePtr == 0L)
        nativePtr = nativeCreate()
    }

    fun startPreview(surface: Surface): Int {
        return nativeStartPreview(nativePtr, surface)
    }

    fun stopPreview() {
        nativeStopPreview(nativePtr)
    }

    fun setPeerConnection(peerConnectionHandle: Long) {
        nativeSetPeerConnection(nativePtr, peerConnectionHandle)
    }

    fun addTransceivers(videoCodec: Int, audioCodec: Int): Int {
        return nativeAddTransceivers(nativePtr, videoCodec, audioCodec)
    }

    fun setupVideoEncoder(config: VideoEncodeConfig): Int {
        val codec = if (config.getUseHevc()) CODEC_H265 else CODEC_H264
        return nativeSetupVideoEncoder(nativePtr, codec,
            config.getWidth(), config.getHeight(),
            config.getBitRate(), config.getFrameRate(),
            config.getIFrameInterval())
    }

    fun setPreviewSurface(surface: Surface?) {
        nativeSetPreviewSurface(nativePtr, surface)
    }

    fun setDecoderSurface(surface: Surface?) {
        nativeSetDecoderSurface(nativePtr, surface)
    }

    fun start(): Int {
        return nativeStart(nativePtr)
    }

    fun stop() {
        nativeStop(nativePtr)
    }

    fun switchCamera() {
        nativeSwitchCamera(nativePtr)
    }

    fun requestKeyFrame() {
        nativeRequestKeyFrame(nativePtr)
    }

    fun getVideoTransceiverHandle(): Long {
        return nativeGetVideoTransceiver(nativePtr)
    }

    fun getAudioTransceiverHandle(): Long {
        return nativeGetAudioTransceiver(nativePtr)
    }

    fun release() {
        if (nativePtr != 0L) {
            nativeRelease(nativePtr)
            nativePtr = 0L
        }
    }

    private external fun nativeCreate(): Long
    private external fun nativeStartPreview(sessionPtr: Long, surface: Surface?): Int
    private external fun nativeStopPreview(sessionPtr: Long)
    private external fun nativeSetPeerConnection(sessionPtr: Long, peerConnectionHandle: Long)
    private external fun nativeAddTransceivers(sessionPtr: Long, videoCodec: Int, audioCodec: Int): Int
    private external fun nativeSetupVideoEncoder(sessionPtr: Long, codec: Int, width: Int, height: Int, bitrate: Int, fps: Int, iframeInterval: Int): Int
    private external fun nativeSetPreviewSurface(sessionPtr: Long, surface: Surface?)
    private external fun nativeSetDecoderSurface(sessionPtr: Long, surface: Surface?)
    private external fun nativeStart(sessionPtr: Long): Int
    private external fun nativeStop(sessionPtr: Long)
    private external fun nativeSwitchCamera(sessionPtr: Long)
    private external fun nativeRequestKeyFrame(sessionPtr: Long)
    private external fun nativeGetVideoTransceiver(sessionPtr: Long): Long
    private external fun nativeGetAudioTransceiver(sessionPtr: Long): Long
    private external fun nativeRelease(sessionPtr: Long)

    companion object {
        const val CODEC_H264 = 1
        const val CODEC_H265 = 6

        init {
            System.loadLibrary("easyrtc_media")
        }
    }
}
