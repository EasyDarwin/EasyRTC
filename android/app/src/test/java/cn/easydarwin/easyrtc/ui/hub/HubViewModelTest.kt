package cn.easydarwin.easyrtc.ui.hub

import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.lifecycle.Observer
import cn.easydarwin.easyrtc.core.result.AppResult
import cn.easydarwin.easyrtc.core.ui.UiState
import cn.easydarwin.easyrtc.domain.model.HubItem
import cn.easydarwin.easyrtc.domain.usecase.ChannelProvider
import cn.easydarwin.easyrtc.domain.usecase.DeviceProvider
import cn.easydarwin.easyrtc.domain.usecase.GetHubItemsUseCase
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test

class HubViewModelTest {

    @get:Rule
    val instantTaskExecutorRule = InstantTaskExecutorRule()

    @Test
    fun load_sets_content_when_use_case_returns_data() {
        val viewModel = HubViewModel(
            GetHubItemsUseCase(
                deviceProvider = DeviceProvider {
                    AppResult.Success(
                        listOf(
                            HubItem.Device(
                                id = "d1",
                                name = "Device",
                                isOnline = true,
                                ip = "10.0.0.1",
                                channelCount = 2,
                                model = "M1",
                                manufacturer = "Vendor"
                            )
                        )
                    )
                },
                channelProvider = ChannelProvider { AppResult.Success(emptyList()) }
            )
        )

        val states = mutableListOf<UiState<List<HubItem>>>()
        val observer = Observer<UiState<List<HubItem>>> { states.add(it) }
        viewModel.uiState.observeForever(observer)
        try {
            viewModel.load()
        } finally {
            viewModel.uiState.removeObserver(observer)
        }

        assertTrue(states.first() is UiState.Loading)
        assertTrue(states.last() is UiState.Content)
        assertEquals(1, (states.last() as UiState.Content).data.size)
    }

    @Test
    fun load_sets_empty_when_use_case_returns_empty_data() {
        val viewModel = HubViewModel(
            GetHubItemsUseCase(
                deviceProvider = DeviceProvider { AppResult.Success(emptyList()) },
                channelProvider = ChannelProvider { AppResult.Success(emptyList()) }
            )
        )

        val states = mutableListOf<UiState<List<HubItem>>>()
        val observer = Observer<UiState<List<HubItem>>> { states.add(it) }
        viewModel.uiState.observeForever(observer)
        try {
            viewModel.load()
        } finally {
            viewModel.uiState.removeObserver(observer)
        }

        assertTrue(states.first() is UiState.Loading)
        assertTrue(states.last() is UiState.Empty)
        assertEquals("No devices or channels available", (states.last() as UiState.Empty).reason)
    }

    @Test
    fun load_sets_error_when_use_case_returns_error() {
        val viewModel = HubViewModel(
            GetHubItemsUseCase(
                deviceProvider = DeviceProvider { AppResult.Error("network failed") },
                channelProvider = ChannelProvider { AppResult.Success(emptyList()) }
            )
        )

        val states = mutableListOf<UiState<List<HubItem>>>()
        val observer = Observer<UiState<List<HubItem>>> { states.add(it) }
        viewModel.uiState.observeForever(observer)
        try {
            viewModel.load()
        } finally {
            viewModel.uiState.removeObserver(observer)
        }

        assertTrue(states.first() is UiState.Loading)
        assertTrue(states.last() is UiState.Error)
        assertEquals("network failed", (states.last() as UiState.Error).message)
    }
}
