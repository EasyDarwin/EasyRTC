package cn.easydarwin.easyrtc.ui.live

sealed class NativePipelineState {
    data object Idle : NativePipelineState()
    data object Starting : NativePipelineState()
    data class Running(val cameraFacing: Int) : NativePipelineState()
    data class Error(val reason: String) : NativePipelineState()
    data object Stopped : NativePipelineState()
}

class NativePipelineController {
    private var _state: NativePipelineState = NativePipelineState.Idle
    val state: NativePipelineState get() = _state

    private var cameraFacing = FACING_BACK

    fun start() {
        if (_state is NativePipelineState.Running) return
        _state = NativePipelineState.Running(cameraFacing)
    }

    fun stop() {
        if (_state is NativePipelineState.Idle || _state is NativePipelineState.Stopped) return
        _state = NativePipelineState.Stopped
    }

    fun switchCamera() {
        if (_state !is NativePipelineState.Running) return
        cameraFacing = if (cameraFacing == FACING_BACK) FACING_FRONT else FACING_BACK
        _state = NativePipelineState.Running(cameraFacing)
    }

    fun reportError(reason: String) {
        _state = NativePipelineState.Error(reason)
    }

    fun release() {
        _state = NativePipelineState.Idle
        cameraFacing = FACING_BACK
    }

    companion object {
        const val FACING_BACK = 0
        const val FACING_FRONT = 1
    }
}
