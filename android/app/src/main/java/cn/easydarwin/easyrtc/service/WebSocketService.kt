package cn.easydarwin.easyrtc.service

import android.app.Service
import android.content.Intent
import android.os.Binder
import android.os.IBinder
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import cn.easydarwin.easyrtc.utils.WebSocketManager
import cn.easyrtc.EasyRTCUser

class WebSocketService : Service() {

    sealed class Event {
        data object Connected : Event()
        data class Message(val text: String) : Event()
        data class Disconnected(val code: Int, val reason: String) : Event()
        data class Error(val throwable: Throwable) : Event()
        data class OnlineUsers(val users: List<EasyRTCUser>) : Event()
        data class Logs(val text: String) : Event()
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
}
