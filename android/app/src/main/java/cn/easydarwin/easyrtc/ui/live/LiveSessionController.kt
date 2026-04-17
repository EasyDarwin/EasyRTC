package cn.easydarwin.easyrtc.ui.live

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData

class LiveSessionController {
    private var currentUser: String? = null

    private val _state = MutableLiveData<LiveUiState>(LiveUiState.Idle)
    val state: LiveData<LiveUiState> = _state

    fun onConnected(user: String?) {
        currentUser = user
        _state.postValue(LiveUiState.Connected(currentUser))
    }

    fun onClosed() {
        _state.postValue(LiveUiState.Disconnected(currentUser))
    }

    fun onFailed(reason: String? = null) {
        _state.postValue(LiveUiState.Failed(currentUser, reason))
    }
}
