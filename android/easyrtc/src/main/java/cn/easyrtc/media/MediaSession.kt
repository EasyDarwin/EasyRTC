package cn.easyrtc.media

import android.util.Log
import android.graphics.SurfaceTexture
import android.view.Surface
import androidx.annotation.Keep
import androidx.lifecycle.MutableLiveData
import cn.easyrtc.EasyRTCLog
import cn.easyrtc.EasyRTCPeerConnectionState
import cn.easyrtc.model.DataChannelEvent
import cn.easyrtc.model.LiveUiState
import cn.easyrtc.model.VideoEncodeConfig
import java.lang.ref.WeakReference


class MediaSession {

    data class InputKbpsStats(
        val videoKbps: Double,
        val audioKbps: Double,
        val totalKbps: Double,
    )

    data class TransceiverFrameStats(
        val videoFramesSent: Long,
        val audioFramesSent: Long,
    )

    private var nativePtr: Long = 0
    private var inputKbpsStatsListener: ((InputKbpsStats) -> Unit)? = null
    private var transceiverFrameStatsListener: ((TransceiverFrameStats) -> Unit)? = null
    private var cameraErrorListener: ((Int) -> Unit)? = null
    private var onOfferCallback: WeakReference<(String) -> Unit>? = null
    private var onAnswerCallback: WeakReference<(String) -> Unit>? = null
    private var currentUser: String? = null

    val connectionState = MutableLiveData<LiveUiState>(LiveUiState.Idle)
    val dataChannel = MutableLiveData<DataChannelEvent>(DataChannelEvent.Idle)
    val remoteVideoSize = MutableLiveData<android.util.Size>(android.util.Size(0, 0))
    val hasCameraError: Boolean
        get() = _cameraHasError

    private var _cameraHasError = false

    fun setInputKbpsStatsListener(listener: ((InputKbpsStats) -> Unit)?) {
        inputKbpsStatsListener = listener
    }

    fun setTransceiverFrameStatsListener(listener: ((TransceiverFrameStats) -> Unit)?) {
        transceiverFrameStatsListener = listener
    }

    fun setCameraErrorListener(listener: ((Int) -> Unit)?) {
        cameraErrorListener = listener
    }

    fun create() {
        assert(nativePtr == 0L)
        nativePtr = nativeCreate()
        EasyRTCLog.i("MediaSession", "create: nativePtr=$nativePtr")
    }

    fun startPreview(surface: Surface): Int {
        _cameraHasError = false
        val ret = nativeStartPreview(nativePtr, surface)
        EasyRTCLog.i("MediaSession", "startPreview: nativePtr=$nativePtr ret=$ret")
        return ret
    }

    fun stopPreview() {
        _cameraHasError = false
        EasyRTCLog.i("MediaSession", "stopPreview: nativePtr=$nativePtr")
        nativeStopPreview(nativePtr)
    }

    fun createPeerConnection(stunUrl: String, turnUrl: String, turnUser: String, turnPass: String): Long {
        val pc = nativeCreatePeerConnection(nativePtr, stunUrl, turnUrl, turnUser, turnPass)
        return pc
    }

    fun releasePeerConnection() {
        EasyRTCLog.i("MediaSession", "releasePeerConnection: nativePtr=$nativePtr")
        nativeReleasePeerConnection(nativePtr)
        connectionState.postValue(LiveUiState.Idle)
    }

    fun createOffer(onSdp: (sdp: String) -> Unit) {
        onOfferCallback = WeakReference(onSdp)
        EasyRTCLog.i("MediaSession", "createOffer: nativePtr=$nativePtr")
        nativeCreateOffer(nativePtr)
    }

    fun createAnswer(offerSdp: String, onSdp: (sdp: String) -> Unit) {
        onAnswerCallback = WeakReference(onSdp)
        EasyRTCLog.i("MediaSession", "createAnswer: nativePtr=$nativePtr")
        nativeCreateAnswer(nativePtr, offerSdp)
    }

    fun setRemoteDescription(sdp: String) {
        EasyRTCLog.i("MediaSession", "setRemoteDescription: nativePtr=$nativePtr")
        nativeSetRemoteDescription(nativePtr, sdp)
    }

    fun addDataChannel(name: String): Long {
        val dc = nativeAddDataChannel(nativePtr, name)
        EasyRTCLog.i("MediaSession", "addDataChannel: nativePtr=$nativePtr name=$name dc=$dc")
        return dc
    }

    fun sendDataChannelMsg(isBinary: Boolean, data: ByteArray): Int {
        return nativeDataChannelSend(nativePtr, isBinary, data)
    }

    fun getIceCandidateType(): Int {
        return nativeGetIceCandidateType(nativePtr)
    }

    fun addTransceivers(videoCodec: Int, audioCodec: Int): Int {
        val i = nativeAddTransceivers(nativePtr, videoCodec, audioCodec)
        EasyRTCLog.i("MediaSession", "addTransceivers: nativePtr=$nativePtr videoCodec=$videoCodec audioCodec=$audioCodec add=$i")
        return i;
    }

    fun startSend(): Int {
        val ret = nativeStartSend(nativePtr)
        EasyRTCLog.i("MediaSession", "startSend: nativePtr=$nativePtr ret=$ret")
        return ret
    }

    fun setupVideoEncoder(config: VideoEncodeConfig): Int {
        val codec = if (config.getUseHevc()) CODEC_H265 else CODEC_H264
        setEncoderRotation(90)
        val ret = nativeSetupVideoEncoderParam(
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
            nativeReleasePeerConnection(nativePtr)
            nativeRelease(nativePtr)
            nativePtr = 0L
        }
    }

    @Keep
    private fun onRemoteVideoSize(width: Int, height: Int) {
        remoteVideoSize.postValue(android.util.Size(width, height))
    }

    @Keep
    private fun onInputKbpsStats(videoKbps: Double, audioKbps: Double, totalKbps: Double) {
        inputKbpsStatsListener?.invoke(InputKbpsStats(videoKbps, audioKbps, totalKbps))
    }

    @Keep
    private fun onTransceiverFrameStats(videoFrames: Long, audioFrames: Long) {
        transceiverFrameStatsListener?.invoke(TransceiverFrameStats(videoFrames, audioFrames))
    }

    @Keep
    private fun onCameraError(error: Int) {
        _cameraHasError = true
        cameraErrorListener?.invoke(error)
    }

    @Keep
    private fun findEncoderForSurfaceInput(mime: String): String {
        val targetColorFormat = android.media.MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface
        EasyRTCLog.i("MediaSession", "findEncoderForSurfaceInput: searching encoders for $mime")
        val codecList = android.media.MediaCodecList(android.media.MediaCodecList.ALL_CODECS)
        val matches = mutableListOf<String>()
        for (codecInfo in codecList.codecInfos) {
            if (!codecInfo.isEncoder) continue
            val caps = try { codecInfo.getCapabilitiesForType(mime) } catch (e: IllegalArgumentException) { null } ?: continue
            val cf = caps.colorFormats
            val cfStr = cf?.joinToString(transform = { "0x%08x".format(it) }) ?: "null"
            val match = cf?.contains(targetColorFormat) == true
            EasyRTCLog.i("MediaSession", "  ${codecInfo.name} colorFormats=[$cfStr] match=$match")
            if (match) matches.add(codecInfo.name)
        }
        EasyRTCLog.w("MediaSession", "findEncoderForSurfaceInput: ${matches.size} matches for $mime")
        return matches.joinToString(";")
    }

    @Keep
    private fun onConnectionStateChangeEvent(state: Int) {
        if (state == EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_NEW ||
            state == EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_CONNECTING) {
            connectionState.postValue(LiveUiState.Connecting(currentUser))
        } else if (state == EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_CONNECTED) {
            currentUser = ""
            val connType = getConnectionTypeDesc()
            connectionState.postValue(LiveUiState.Connected(currentUser, connType))
        } else if (state == EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_FAILED) {
            connectionState.postValue(LiveUiState.Failed(currentUser, null))
        } else if (state == EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_CLOSED ||
                   state == EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_DISCONNECTED) {
            connectionState.postValue(LiveUiState.Disconnected(currentUser))
        }
    }

    private fun getConnectionTypeDesc(): String {
        val type = nativeGetIceCandidateType(nativePtr)
        return if (type <= 2) "P2P直连" else "Relay中转"
    }

    @Keep
    private fun onSdpEvent(isOffer: Int, sdp: String) {
        if (isOffer == 1) {
            val cb = onOfferCallback; onOfferCallback = null
            cb?.get()?.invoke(sdp)
        } else {
            val cb = onAnswerCallback; onAnswerCallback = null
            cb?.get()?.invoke(sdp)
        }
    }

    @Keep
    private fun onDataChannelEvent(type: Int, binary: Int, data: ByteArray) {
        if (type == 1) dataChannel.postValue(DataChannelEvent.Open)
        else dataChannel.postValue(DataChannelEvent.Message(binary, data.copyOf(data.size), data.size))
    }

    private external fun nativeCreate(): Long
    private external fun nativeStartPreview(sessionPtr: Long, surface: Surface?): Int
    private external fun nativeStopPreview(sessionPtr: Long)

    private external fun nativeAddTransceivers(
        sessionPtr: Long,
        videoCodec: Int,
        audioCodec: Int
    ): Int

    private external fun nativeSetupVideoEncoderParam(
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
    private external fun nativeSwitchCamera(sessionPtr: Long)
    private external fun nativeRequestKeyFrame(sessionPtr: Long)
    private external fun nativeRelease(sessionPtr: Long)

    private external fun nativeCreatePeerConnection(
        sessionPtr: Long,
        stunUrl: String,
        turnUrl: String,
        turnUser: String,
        turnPass: String
    ): Long
    private external fun nativeReleasePeerConnection(sessionPtr: Long)
    private external fun nativeCreateOffer(sessionPtr: Long)
    private external fun nativeCreateAnswer(sessionPtr: Long, offerSdp: String)
    private external fun nativeSetRemoteDescription(sessionPtr: Long, sdp: String)
    private external fun nativeAddDataChannel(sessionPtr: Long, name: String): Long
    private external fun nativeDataChannelSend(sessionPtr: Long, isBinary: Boolean, data: ByteArray): Int
    private external fun nativeGetIceCandidateType(sessionPtr: Long): Int

    companion object {
        const val CODEC_H264 = 1
        const val CODEC_H265 = 7

        @JvmStatic
        fun getSdkVersion(): String = nativeGetVersion()

        private external fun nativeGetVersion(): String

        init {
            System.loadLibrary("easyrtc_media")
        }
    }
}
