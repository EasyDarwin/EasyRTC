package cn.easyrtc.helper

import android.annotation.SuppressLint
import android.content.Context
import android.hardware.camera2.CameraCaptureSession
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraManager
import android.hardware.camera2.CaptureRequest
import android.os.Handler
import android.os.HandlerThread
import android.util.Log
import android.view.Surface

class CameraHelperV2(
    private val context: Context,
) {
    interface Listener {
        fun onStarted()
        fun onError(msg: String)
    }

    private val tag = "CameraHelperV2"
    private val cameraManager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager
    private var cameraDevice: CameraDevice? = null
    private var captureSession: CameraCaptureSession? = null
    private var cameraThread: HandlerThread? = null
    private var cameraHandler: Handler? = null

    private var currentFacing: Int = CameraCharacteristics.LENS_FACING_FRONT

    private fun ensureThread() {
        if (cameraThread != null) return
        cameraThread = HandlerThread("rtc-camera2").also { it.start() }
        cameraHandler = Handler(cameraThread!!.looper)
    }

    fun setFacingFront(front: Boolean) {
        currentFacing = if (front) CameraCharacteristics.LENS_FACING_FRONT else CameraCharacteristics.LENS_FACING_BACK
    }

    @SuppressLint("MissingPermission")
    fun start(previewSurface: Surface, encoderSurface: Surface, listener: Listener) {
        ensureThread()
        val id = findCameraId(currentFacing)
        if (id == null) {
            listener.onError("No camera found for facing=$currentFacing")
            return
        }
        cameraManager.openCamera(id, object : CameraDevice.StateCallback() {
            override fun onOpened(camera: CameraDevice) {
                cameraDevice = camera
                createSession(camera, previewSurface, encoderSurface, listener)
            }

            override fun onDisconnected(camera: CameraDevice) {
                Log.w(tag, "Camera disconnected")
                stop()
            }

            override fun onError(camera: CameraDevice, error: Int) {
                listener.onError("openCamera error=$error")
                stop()
            }
        }, cameraHandler)
    }

    private fun createSession(
        camera: CameraDevice,
        previewSurface: Surface,
        encoderSurface: Surface,
        listener: Listener,
    ) {
        val targets = listOf(previewSurface, encoderSurface)
        camera.createCaptureSession(targets, object : CameraCaptureSession.StateCallback() {
            override fun onConfigured(session: CameraCaptureSession) {
                captureSession = session
                val req = camera.createCaptureRequest(CameraDevice.TEMPLATE_RECORD).apply {
                    addTarget(previewSurface)
                    addTarget(encoderSurface)
                    set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO)
                }
                session.setRepeatingRequest(req.build(), null, cameraHandler)
                listener.onStarted()
            }

            override fun onConfigureFailed(session: CameraCaptureSession) {
                listener.onError("createCaptureSession failed")
            }
        }, cameraHandler)
    }

    fun stop() {
        try {
            captureSession?.stopRepeating()
        } catch (_: Exception) {
        }
        captureSession?.close()
        captureSession = null
        cameraDevice?.close()
        cameraDevice = null
        cameraThread?.quitSafely()
        cameraThread = null
        cameraHandler = null
    }

    private fun findCameraId(facing: Int): String? {
        for (id in cameraManager.cameraIdList) {
            val ch = cameraManager.getCameraCharacteristics(id)
            if (ch.get(CameraCharacteristics.LENS_FACING) == facing) {
                return id
            }
        }
        return null
    }
}
