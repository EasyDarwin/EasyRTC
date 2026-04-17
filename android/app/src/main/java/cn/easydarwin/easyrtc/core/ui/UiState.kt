package cn.easydarwin.easyrtc.core.ui

sealed class UiState<out T> {
    object Loading : UiState<Nothing>()
    data class Content<out T>(val data: T) : UiState<T>()
    data class Empty(val reason: String) : UiState<Nothing>()
    data class Error(val message: String) : UiState<Nothing>()
}
