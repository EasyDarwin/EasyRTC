package cn.easydarwin.easyrtc.ui.hub

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import cn.easydarwin.easyrtc.core.ui.UiState
import cn.easyrtc.EasyRTCUser

class HubViewModel : ViewModel() {

    private val _uiState = MutableLiveData<UiState<List<EasyRTCUser>>>(UiState.Loading)
    val uiState: LiveData<UiState<List<EasyRTCUser>>> = _uiState

    fun updateUsers(users: List<EasyRTCUser>) {
        _uiState.value = if (users.isEmpty()) {
            UiState.Empty("No online users")
        } else {
            UiState.Content(users)
        }
    }

    fun setLoading() {
        _uiState.value = UiState.Loading
    }
}
