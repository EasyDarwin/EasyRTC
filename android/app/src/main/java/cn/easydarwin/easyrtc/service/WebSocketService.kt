package cn.easydarwin.easyrtc.service

import android.app.Service
import android.content.Intent
import android.os.Binder
import android.os.IBinder
import android.util.Log
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import cn.easydarwin.easyrtc.utils.AppLogStore
import cn.easydarwin.easyrtc.utils.CallLog
import cn.easydarwin.easyrtc.utils.SPUtil
import cn.easydarwin.easyrtc.utils.WebSocketManager
import cn.easydarwin.easyrtc.utils.WebSocketManager.Companion.HPNTIWEBRTCOFFERINFO2
import cn.easydarwin.easyrtc.utils.WebSocketManager.Companion.HPREQGETWEBRTCOFFERINFO
import cn.easyrtc.EasyRTCCodec
import cn.easyrtc.EasyRTCUser

class WebSocketService : Service() {

    sealed class Event {
        data object Connected : Event()
        data class Message(val text: String) : Event()
        data class Disconnected(val code: Int, val reason: String) : Event()
        data class Error(val throwable: Throwable) : Event()
        data class OnlineUsers(val users: List<EasyRTCUser>) : Event()
        data class Logs(val text: String) : Event()
        data class IncomingCall(val uuid: String, val data: ByteArray, val callout: Boolean = false, var handled: Boolean = false) : Event()
        data class AnswerSDP(val sdp: String, var handled: Boolean = false) : Event()
    }

    inner class LocalBinder : Binder() {
        fun getService(): WebSocketService = this@WebSocketService
    }

    private val binder = LocalBinder()
    private val _events = MutableLiveData<Event>()
    val events: LiveData<Event> = _events

    private val manager: WebSocketManager by lazy {
        WebSocketManager(
            url = "ws://rts.easyrtc.cn:19000",
            token = "",
            callback = object : WebSocketManager.WebSocketCallback {
                override fun onWSConnected() {
                    _events.postValue(Event.Connected)
                }

                override fun onWSMessage(text: String) {
                    _events.postValue(Event.Message(text))
                }

                override fun onWSDisconnected(code: Int, reason: String) {
                    _events.postValue(Event.Disconnected(code, reason))
                }

                override fun onWSError(error: Throwable) {
                    _events.postValue(Event.Error(error))
                }

                override fun onWSOnlineUsers(users: List<EasyRTCUser>) {
                    _events.postValue(Event.OnlineUsers(users))
                }

                override fun onWSLogs(txt: String) {
                    _events.postValue(Event.Logs(txt))
                }

                override fun onWSIncomingCall(uuid: String) {
//                    _events.postValue(Event.IncomingCall(uuid))
                }

                override fun onWSAnswerSDP(sdp: String) {
                    _events.postValue(Event.AnswerSDP(sdp))
                }

                override fun onTypeAndData(type: Int, data: ByteArray) {
                    val uuid = manager.byteArrayToUuid(data.copyOfRange(8, 24))
                    if (HPREQGETWEBRTCOFFERINFO == type) {
                        _events.postValue(Event.IncomingCall(uuid, data))
                    } else if (HPNTIWEBRTCOFFERINFO2 == type) {
                        _events.postValue(Event.IncomingCall(uuid, data, true))
                    }
                }
            }
        )
    }

    override fun onCreate() {
        super.onCreate()
        manager.connect()
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onDestroy() {
        manager.shutdown()
        super.onDestroy()
    }

    fun call(uuid: String) {
        manager.call(uuid)
    }

    fun sendOfferSDP(sdp: String, isOffer: Boolean) {
        manager.sendOfferSDP(sdp, isOffer)
    }

    fun getOnlineUsers() {
        manager.getOnlineClients()
    }

    data class CallSetup(
        val callout: Boolean,
        val sdp: String,
        val videoCodec: Int,
        val audioCodec: Int,
        val hasDataChannel: Boolean,
        val dataChannelName: String,
        val stunTurnInfo: WebSocketManager.StunTurnInfo
    )

    fun prepareIncomingCall(data: ByteArray, callout: Boolean): CallSetup {
        var st = WebSocketManager.StunTurnInfo()
        if (callout) {
            val sdp = manager.handlerPeerConnection(data, true, st)
            CallLog.append("offer from remote:\n$sdp")
            var videoCodec = 0
            var audioCodec = 0
            if (sdp.contains("m=video", ignoreCase = true)) {
                when {
                    sdp.contains("H264/90000") -> videoCodec = EasyRTCCodec.H264
                    sdp.contains("H265/90000") -> videoCodec = EasyRTCCodec.H265
                    sdp.contains("VP8/90000") -> videoCodec = EasyRTCCodec.VP8
                }
            }
            if (sdp.contains("m=audio", ignoreCase = true)) {
                when {
                    sdp.contains("PCMA/8000", ignoreCase = true) -> audioCodec = EasyRTCCodec.ALAW
                    sdp.contains("PCMU/8000", ignoreCase = true) -> audioCodec = EasyRTCCodec.MULAW
                    sdp.contains("opus/48000", ignoreCase = true) -> audioCodec = EasyRTCCodec.OPUS
                }
            }
            val hasDC = sdp.contains("webrtc-datachannel", ignoreCase = true)
            return CallSetup(callout = true, sdp = sdp, videoCodec = videoCodec, audioCodec = audioCodec, hasDataChannel = hasDC, dataChannelName = "", st)
        }
        manager.handlerPeerConnection(data, false, st)
        val videoCodeID = if (SPUtil.getInstance().getIsHevc()) EasyRTCCodec.H265 else EasyRTCCodec.H264
        return CallSetup(callout = false, sdp = "", videoCodec = videoCodeID, audioCodec = EasyRTCCodec.ALAW, hasDataChannel = true, dataChannelName = "123", st)
    }
}
