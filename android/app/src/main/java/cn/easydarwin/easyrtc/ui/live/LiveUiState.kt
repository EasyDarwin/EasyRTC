package cn.easydarwin.easyrtc.ui.live

sealed class LiveUiState {
    data object Idle : LiveUiState()
    data class Connected(val user: String?) : LiveUiState()
    data class Disconnected(val user: String?) : LiveUiState()
    data class Failed(val user: String?, val reason: String?) : LiveUiState()
}
