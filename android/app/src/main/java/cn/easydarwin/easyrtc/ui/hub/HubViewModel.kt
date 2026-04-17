package cn.easydarwin.easyrtc.ui.hub

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import cn.easydarwin.easyrtc.core.result.AppResult
import cn.easydarwin.easyrtc.core.ui.UiState
import cn.easydarwin.easyrtc.domain.model.HubItem
import cn.easydarwin.easyrtc.domain.usecase.GetHubItemsUseCase

class HubViewModel(
    private val getHubItemsUseCase: GetHubItemsUseCase
) : ViewModel() {

    private val _uiState = MutableLiveData<UiState<List<HubItem>>>(UiState.Loading)
    val uiState: LiveData<UiState<List<HubItem>>> = _uiState

    fun load() {
        _uiState.value = UiState.Loading

        when (val result = getHubItemsUseCase.execute()) {
            is AppResult.Success -> {
                val items = result.data
                _uiState.value = if (items.isEmpty()) {
                    UiState.Empty("No devices or channels available")
                } else {
                    UiState.Content(items)
                }
            }

            is AppResult.Error -> {
                _uiState.value = UiState.Error(result.message)
            }
        }
    }
}
