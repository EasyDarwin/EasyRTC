package cn.easyrtc.media

import android.util.Log
import android.graphics.SurfaceTexture
import android.view.Surface
import androidx.annotation.Keep
import cn.easyrtc.EasyRTCLog
import cn.easyrtc.model.VideoEncodeConfig

class MediaSession {

    data class InputKbpsStats(
        val videoKbps: Double,
        val audioKbps: Double,
        val totalKbps: Double,
    )

    private var nativePtr: Long = 0
    private var inputKbpsStatsListener: ((InputKbpsStats) -> Unit)? = null

    fun setInputKbpsStatsListener(listener: ((InputKbpsStats) -> Unit)?) {
        inputKbpsStatsListener = listener
    }

    fun create() {
        assert(nativePtr == 0L)
        nativePtr = nativeCreate()
        EasyRTCLog.i("MediaSession", "create: nativePtr=$nativePtr")
    }

    fun startPreview(surface: Surface): Int {
        val ret = nativeStartPreview(nativePtr, surface)
        EasyRTCLog.i("MediaSession", "startPreview: nativePtr=$nativePtr ret=$ret")
        return ret
    }

    fun stopPreview() {
        EasyRTCLog.i("MediaSession", "stopPreview: nativePtr=$nativePtr")
        nativeStopPreview(nativePtr)
    }

    fun setPeerConnection(peerConnectionHandle: Long) {
        EasyRTCLog.i("MediaSession", "setPeerConnection: nativePtr=$nativePtr peer=$peerConnectionHandle")
        nativeSetPeerConnection(nativePtr, peerConnectionHandle)
    }

    fun addTransceivers(videoCodec: Int, audioCodec: Int): Int {
        val i = nativeAddTransceivers(nativePtr, videoCodec, audioCodec)
        val send = nativeStartSend(nativePtr)
        val recv = nativeStartRecv(nativePtr)
        Log.i("MS", "addTrans:$i, startSend:$send, startRecv:$recv")
        EasyRTCLog.i("MediaSession", "addTransceivers: nativePtr=$nativePtr videoCodec=$videoCodec audioCodec=$audioCodec add=$i send=$send recv=started")
        return i;
    }

    fun addSendOnlyTransceivers(videoCodec: Int, audioCodec: Int): Int {
        val i = nativeAddTransceivers(nativePtr, videoCodec, audioCodec)
        val send = nativeStartSend(nativePtr)
        Log.i("MS", "addSendOnlyTrans:$i, startSend:$send")
        EasyRTCLog.i("MediaSession", "addSendOnlyTransceivers: nativePtr=$nativePtr videoCodec=$videoCodec audioCodec=$audioCodec add=$i send=$send")
        return i
    }

    fun removeTransceivers() {
        EasyRTCLog.i("MediaSession", "removeTransceivers: nativePtr=$nativePtr")
        nativeStopRecv(nativePtr)
        nativeStopSend(nativePtr)
        nativeRemoveTransceivers(nativePtr);
    }

    fun setupVideoEncoder(config: VideoEncodeConfig): Int {
        val codec = if (config.getUseHevc()) CODEC_H265 else CODEC_H264
        setEncoderRotation(90)
        val ret = nativeSetupVideoEncoder(
            nativePtr, codec,
            config.getWidth(), config.getHeight(),
            config.getBitRate(), config.getFrameRate(),
            config.getIFrameInterval()
        )
        EasyRTCLog.i(
            "MediaSession",
            "setupVideoEncoder: nativePtr=$nativePtr codec=$codec ${config.getWidth()}x${config.getHeight()} bitrate=${config.getBitRate()} fps=${config.getFrameRate()} ret=$ret"
        )
        return ret
    }

    fun setEncoderRotation(rotation: Int) {
        EasyRTCLog.d("MediaSession", "setEncoderRotation: nativePtr=$nativePtr rotation=$rotation")
        nativeSetEncoderRotation(nativePtr, rotation)
    }

    fun setDeviceId(deviceId: String) {
        EasyRTCLog.i("MediaSession", "setDeviceId: nativePtr=$nativePtr deviceId=$deviceId")
        nativeSetDeviceId(nativePtr, deviceId)
    }

    fun setConnectState(state: Int) {
        if (nativePtr == 0L) return;
        EasyRTCLog.i("MediaSession", "setConnectState: nativePtr=$nativePtr state=$state")
        nativeSetState(nativePtr, state);
    }

    fun setDecoderSurface(surface: Surface?) {
        nativeSetDecoderSurface(nativePtr, surface)
    }

    @Keep
    fun createCameraInputSurfaceTexture(texId: Int, width: Int, height: Int): SurfaceTexture {
        return SurfaceTexture(texId).also {
            it.setDefaultBufferSize(width, height)
        }
    }

    fun switchCamera() {
        EasyRTCLog.i("MediaSession", "switchCamera: nativePtr=$nativePtr")
        nativeSwitchCamera(nativePtr)
    }

    fun requestKeyFrame() {
        if (nativePtr == 0L) return;
        EasyRTCLog.i("MediaSession", "requestKeyFrame: nativePtr=$nativePtr")
        nativeRequestKeyFrame(nativePtr)
    }

    fun release() {
        if (nativePtr != 0L) {
            EasyRTCLog.i("MediaSession", "release: nativePtr=$nativePtr")
            nativeStopRecv(nativePtr)
            nativeStopSend(nativePtr)
            nativeRemoveTransceivers(nativePtr);
            nativeRelease(nativePtr)
            nativePtr = 0L
        }
    }

    @Keep
    private fun onRemoteVideoSize(width: Int, height: Int) {
        cn.easyrtc.EasyRTCSdk.notifyRemoteVideoSize(width, height)
    }

    @Keep
    private fun onInputKbpsStats(videoKbps: Double, audioKbps: Double, totalKbps: Double) {
        inputKbpsStatsListener?.invoke(InputKbpsStats(videoKbps, audioKbps, totalKbps))
    }

    private external fun nativeCreate(): Long
    private external fun nativeStartPreview(sessionPtr: Long, surface: Surface?): Int
    private external fun nativeStopPreview(sessionPtr: Long)

    private external fun nativeSetState(sessionPtr: Long, state: Int)
    private external fun nativeSetPeerConnection(sessionPtr: Long, peerConnectionHandle: Long)
    private external fun nativeAddTransceivers(
        sessionPtr: Long,
        videoCodec: Int,
        audioCodec: Int
    ): Int

    private external fun nativeRemoveTransceivers(sessionPtr: Long)
    private external fun nativeSetupVideoEncoder(
        sessionPtr: Long,
        codec: Int,
        width: Int,
        height: Int,
        bitrate: Int,
        fps: Int,
        iframeInterval: Int
    ): Int
    private external fun nativeSetEncoderRotation(sessionPtr: Long, rotation: Int)
    private external fun nativeSetDeviceId(sessionPtr: Long, deviceId: String)

    private external fun nativeSetDecoderSurface(sessionPtr: Long, surface: Surface?)
    private external fun nativeStartSend(sessionPtr: Long): Int
    private external fun nativeStartRecv(sessionPtr: Long)
    private external fun nativeStopSend(sessionPtr: Long)
    private external fun nativeStopRecv(sessionPtr: Long)
    private external fun nativeSwitchCamera(sessionPtr: Long)
    private external fun nativeRequestKeyFrame(sessionPtr: Long)
    private external fun nativeRelease(sessionPtr: Long)

    companion object {
        const val CODEC_H264 = 1
        const val CODEC_H265 = 6

        init {
            System.loadLibrary("easyrtc_media")
        }
    }
}
