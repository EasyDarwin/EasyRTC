package cn.easydarwin.easyrtc.ui.live

import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.lifecycle.Observer
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test

class LiveSessionControllerTest {

    @get:Rule
    val instantTaskExecutorRule = InstantTaskExecutorRule()

    @Test
    fun connected_then_closed_transitions_to_disconnected_with_same_user() {
        val controller = LiveSessionController()
        val states = mutableListOf<LiveUiState>()

        val observer = Observer<LiveUiState> { states.add(it) }
        controller.state.observeForever(observer)
        try {
            controller.onConnected("alice")
            controller.onClosed()
        } finally {
            controller.state.removeObserver(observer)
        }

        assertTrue(states[1] is LiveUiState.Connected)
        assertTrue(states[2] is LiveUiState.Disconnected)
        assertEquals("alice", (states[2] as LiveUiState.Disconnected).user)
    }
}
