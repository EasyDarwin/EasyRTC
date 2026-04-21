package cn.easyrtc.media

import android.view.Surface

class MediaSession {

    private var nativePtr: Long = 0

    fun create(peerConnectionHandle: Long) {
        assert(nativePtr == 0L)
        nativePtr = nativeCreate(peerConnectionHandle)
    }

    fun addTransceivers(videoCodec: Int, audioCodec: Int): Int {
        return nativeAddTransceivers(nativePtr, videoCodec, audioCodec)
    }

    fun setupVideoEncoder(
        codec: Int = 1,
        width: Int = 720,
        height: Int = 1280,
        bitrate: Int = 2000000,
        fps: Int = 30,
        iframeInterval: Int = 1
    ): Int {
        return nativeSetupVideoEncoder(nativePtr, codec, width, height, bitrate, fps, iframeInterval)
    }

    fun createEncoderSurface(): Surface? {
        return nativeCreateEncoderSurface(nativePtr)
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

    private external fun nativeCreate(peerConnectionHandle: Long): Long
    private external fun nativeAddTransceivers(sessionPtr: Long, videoCodec: Int, audioCodec: Int): Int
    private external fun nativeSetupVideoEncoder(sessionPtr: Long, codec: Int, width: Int, height: Int, bitrate: Int, fps: Int, iframeInterval: Int): Int
    private external fun nativeCreateEncoderSurface(sessionPtr: Long): Surface?
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
        init {
            System.loadLibrary("easyrtc_media")
        }
    }
}
