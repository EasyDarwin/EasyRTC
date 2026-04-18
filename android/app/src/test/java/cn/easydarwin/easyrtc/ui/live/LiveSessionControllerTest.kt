package cn.easydarwin.easyrtc.ui.live

import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.lifecycle.LiveData
import androidx.lifecycle.Observer
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test

class LiveSessionControllerTest {

    @get:Rule
    val instantTaskExecutorRule = InstantTaskExecutorRule()

    @Test
    fun initial_state_is_idle() {
        val controller = LiveSessionController()
        assertTrue(controller.state.value is LiveUiState.Idle)
    }

    @Test
    fun connected_transitions_from_idle() {
        val controller = LiveSessionController()
        val states = collectLiveStates(controller.state) {
            controller.onConnected("alice")
        }

        assertEquals(2, states.size)
        assertTrue(states[0] is LiveUiState.Idle)
        assertTrue(states[1] is LiveUiState.Connected)
        assertEquals("alice", (states[1] as LiveUiState.Connected).user)
    }

    @Test
    fun connected_then_closed_transitions_to_disconnected_with_same_user() {
        val controller = LiveSessionController()
        val states = collectLiveStates(controller.state) {
            controller.onConnected("alice")
            controller.onClosed()
        }

        assertTrue(states[1] is LiveUiState.Connected)
        assertTrue(states[2] is LiveUiState.Disconnected)
        assertEquals("alice", (states[2] as LiveUiState.Disconnected).user)
    }

    @Test
    fun connected_then_failed_transitions_with_reason() {
        val controller = LiveSessionController()
        val states = collectLiveStates(controller.state) {
            controller.onConnected("bob")
            controller.onFailed("timeout")
        }

        assertTrue(states[1] is LiveUiState.Connected)
        assertTrue(states[2] is LiveUiState.Failed)
        assertEquals("bob", (states[2] as LiveUiState.Failed).user)
        assertEquals("timeout", (states[2] as LiveUiState.Failed).reason)
    }

    @Test
    fun failed_without_connect_has_null_user() {
        val controller = LiveSessionController()
        val states = collectLiveStates(controller.state) {
            controller.onFailed("error")
        }

        assertTrue(states[1] is LiveUiState.Failed)
        assertEquals(null, (states[1] as LiveUiState.Failed).user)
        assertEquals("error", (states[1] as LiveUiState.Failed).reason)
    }

    @Test
    fun closed_without_connect_has_null_user() {
        val controller = LiveSessionController()
        val states = collectLiveStates(controller.state) {
            controller.onClosed()
        }

        assertTrue(states[1] is LiveUiState.Disconnected)
        assertEquals(null, (states[1] as LiveUiState.Disconnected).user)
    }

    @Test
    fun reconnect_updates_user() {
        val controller = LiveSessionController()
        val states = collectLiveStates(controller.state) {
            controller.onConnected("alice")
            controller.onClosed()
            controller.onConnected("bob")
        }

        assertTrue(states[1] is LiveUiState.Connected)
        assertEquals("alice", (states[1] as LiveUiState.Connected).user)
        assertTrue(states[3] is LiveUiState.Connected)
        assertEquals("bob", (states[3] as LiveUiState.Connected).user)
    }

    @Test
    fun failed_with_null_reason_is_valid() {
        val controller = LiveSessionController()
        val states = collectLiveStates(controller.state) {
            controller.onFailed(null)
        }

        assertTrue(states[1] is LiveUiState.Failed)
        assertEquals(null, (states[1] as LiveUiState.Failed).reason)
    }

    private fun collectLiveStates(
        liveData: LiveData<LiveUiState>,
        block: () -> Unit
    ): List<LiveUiState> {
        val states = mutableListOf<LiveUiState>()
        val observer = Observer<LiveUiState> { states.add(it) }
        liveData.observeForever(observer)
        try {
            block()
        } finally {
            liveData.removeObserver(observer)
        }
        return states
    }
}
