package cn.easydarwin.easyrtc.ui.live

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class NativePipelineControllerTest {

    @Test
    fun initial_state_is_idle() {
        val controller = NativePipelineController()
        assertTrue(controller.state is NativePipelineState.Idle)
    }

    @Test
    fun start_transitions_to_running_with_back_camera() {
        val controller = NativePipelineController()
        controller.start()

        assertTrue(controller.state is NativePipelineState.Running)
        assertEquals(NativePipelineController.FACING_BACK, (controller.state as NativePipelineState.Running).cameraFacing)
    }

    @Test
    fun start_when_already_running_is_noop() {
        val controller = NativePipelineController()
        controller.start()
        val firstState = controller.state

        controller.start()
        assertEquals(firstState, controller.state)
    }

    @Test
    fun stop_transitions_from_running_to_stopped() {
        val controller = NativePipelineController()
        controller.start()
        controller.stop()

        assertTrue(controller.state is NativePipelineState.Stopped)
    }

    @Test
    fun stop_when_idle_is_noop() {
        val controller = NativePipelineController()
        controller.stop()

        assertTrue(controller.state is NativePipelineState.Idle)
    }

    @Test
    fun stop_when_already_stopped_is_noop() {
        val controller = NativePipelineController()
        controller.start()
        controller.stop()
        val stoppedState = controller.state

        controller.stop()
        assertEquals(stoppedState, controller.state)
    }

    @Test
    fun switch_camera_flips_facing_while_running() {
        val controller = NativePipelineController()
        controller.start()

        assertEquals(NativePipelineController.FACING_BACK, (controller.state as NativePipelineState.Running).cameraFacing)

        controller.switchCamera()
        assertEquals(NativePipelineController.FACING_FRONT, (controller.state as NativePipelineState.Running).cameraFacing)

        controller.switchCamera()
        assertEquals(NativePipelineController.FACING_BACK, (controller.state as NativePipelineState.Running).cameraFacing)
    }

    @Test
    fun switch_camera_when_not_running_is_noop() {
        val controller = NativePipelineController()
        controller.switchCamera()

        assertTrue(controller.state is NativePipelineState.Idle)
    }

    @Test
    fun switch_camera_when_stopped_is_noop() {
        val controller = NativePipelineController()
        controller.start()
        controller.stop()
        controller.switchCamera()

        assertTrue(controller.state is NativePipelineState.Stopped)
    }

    @Test
    fun report_error_transitions_to_error_state() {
        val controller = NativePipelineController()
        controller.start()
        controller.reportError("camera open failed")

        assertTrue(controller.state is NativePipelineState.Error)
        assertEquals("camera open failed", (controller.state as NativePipelineState.Error).reason)
    }

    @Test
    fun report_error_from_idle_transitions_to_error_state() {
        val controller = NativePipelineController()
        controller.reportError("no camera")

        assertTrue(controller.state is NativePipelineState.Error)
        assertEquals("no camera", (controller.state as NativePipelineState.Error).reason)
    }

    @Test
    fun release_resets_to_idle_and_back_camera() {
        val controller = NativePipelineController()
        controller.start()
        controller.switchCamera()
        controller.release()

        assertTrue(controller.state is NativePipelineState.Idle)

        controller.start()
        assertEquals(NativePipelineController.FACING_BACK, (controller.state as NativePipelineState.Running).cameraFacing)
    }

    @Test
    fun full_lifecycle_start_switch_stop_release() {
        val controller = NativePipelineController()

        assertTrue(controller.state is NativePipelineState.Idle)

        controller.start()
        assertTrue(controller.state is NativePipelineState.Running)
        assertEquals(NativePipelineController.FACING_BACK, (controller.state as NativePipelineState.Running).cameraFacing)

        controller.switchCamera()
        assertEquals(NativePipelineController.FACING_FRONT, (controller.state as NativePipelineState.Running).cameraFacing)

        controller.stop()
        assertTrue(controller.state is NativePipelineState.Stopped)

        controller.release()
        assertTrue(controller.state is NativePipelineState.Idle)
    }
}
