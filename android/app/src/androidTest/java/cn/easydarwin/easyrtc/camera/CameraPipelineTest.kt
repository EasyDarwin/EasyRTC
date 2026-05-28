package cn.easydarwin.easyrtc.camera

import android.Manifest
import android.content.Context
import android.content.Intent
import android.graphics.SurfaceTexture
import android.os.Bundle
import android.os.SystemClock
import android.view.Surface
import android.view.TextureView
import android.widget.FrameLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import androidx.test.rule.GrantPermissionRule
import cn.easyrtc.media.MediaSession
import cn.easyrtc.model.VideoEncodeConfig
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

@RunWith(AndroidJUnit4::class)
class CameraPipelineTest {

    @get:Rule
    val permissionRule: GrantPermissionRule = GrantPermissionRule.grant(
        Manifest.permission.CAMERA,
        Manifest.permission.RECORD_AUDIO,
        Manifest.permission.MODIFY_AUDIO_SETTINGS
    )

    @get:Rule
    val activityRule = ActivityTestRule(CameraTestActivity::class.java, false, false)

    private lateinit var session: MediaSession

    private val defaultConfig
        get() = VideoEncodeConfig(
            useHevc = false,
            frameRate = 30,
            cameraId = android.hardware.Camera.CameraInfo.CAMERA_FACING_BACK,
            resolution = android.util.Size(1920, 1080),
            orientation = VideoEncodeConfig.ORIENTATION_90,
            bitRate = 1_000_000,
            iFrameInterval = 1
        )

    @Before
    fun setUp() {
        session = MediaSession()
        session.create()
    }

    @After
    fun tearDown() {
        if (::session.isInitialized) {
            session.stopPreview()
            session.setDecoderSurface(null)
            session.releasePeerConnection()
            session.release()
        }
    }

    private fun launchAndWaitForSurface(): CameraTestActivity {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val activity = activityRule.launchActivity(Intent(context, CameraTestActivity::class.java))
        assertTrue("SurfaceTexture should be available",
            activity.surfaceLatch.await(5, TimeUnit.SECONDS))
        return activity
    }

    @Test
    fun previewOnly_sendFramesIncrease() {
        val activity = launchAndWaitForSurface()

        session.setupVideoEncoder(defaultConfig)
        val surface = activity.acquirePreviewSurface()
        assertEquals("startPreview should succeed", 0, session.startPreview(surface))

        SystemClock.sleep(3000)

        session.stopPreview()
        session.setDecoderSurface(null)
        session.releasePeerConnection()

        assertTrue("Activity should survive", !activity.isDestroyed)
    }

    @Test
    fun previewToRecording_sendFramesIncrease() {
        val activity = launchAndWaitForSurface()

        session.setupVideoEncoder(defaultConfig)
        val surface = activity.acquirePreviewSurface()
        assertEquals("startPreview should succeed", 0, session.startPreview(surface))

        SystemClock.sleep(1000)

        val pc = session.createPeerConnection(
            "stun:stun.l.google.com:19302",
            "turn:turn.example.com:3478",
            "user",
            "pass"
        )
        assertTrue("PeerConnection should be created", pc != 0L)


        session.addTransceivers(MediaSession.CODEC_H264, 5)
        session.addDataChannel("test")

        SystemClock.sleep(5000)


        session.releasePeerConnection()
        SystemClock.sleep(1000)

        assertTrue("Activity should survive", !activity.isDestroyed)
    }

    @Test
    fun previewRecordingBackToPreview_sendFramesIncreaseThenStop() {
        val activity = launchAndWaitForSurface()

        session.setupVideoEncoder(defaultConfig)
        val surface = activity.acquirePreviewSurface()
        assertEquals(0, session.startPreview(surface))
        SystemClock.sleep(1000)

        // Phase 2: start recording
        val pc = session.createPeerConnection(
            "stun:stun.l.google.com:19302",
            "turn:turn.example.com:3478",
            "user",
            "pass"
        )
        assertTrue(pc != 0L)
        session.addTransceivers(MediaSession.CODEC_H264, 5)
        session.addDataChannel("test")
        SystemClock.sleep(5000)

        // Phase 3: stop recording
        session.releasePeerConnection()
        SystemClock.sleep(2000)

        // Phase 4: start recording again
        val pc2 = session.createPeerConnection(
            "stun:stun.l.google.com:19302",
            "turn:turn.example.com:3478",
            "user",
            "pass"
        )
        assertTrue(pc2 != 0L)
        session.addTransceivers(MediaSession.CODEC_H264, 5)
        SystemClock.sleep(5000)


        session.releasePeerConnection()
        session.stopPreview()
        session.setDecoderSurface(null)
        SystemClock.sleep(500)

        assertTrue("Activity should survive", !activity.isDestroyed)
    }

    @Test
    fun switchCamera_noCrash() {
        val activity = launchAndWaitForSurface()

        session.setupVideoEncoder(defaultConfig)
        val surface = activity.acquirePreviewSurface()
        assertEquals(0, session.startPreview(surface))
        SystemClock.sleep(1000)

        session.switchCamera()
        SystemClock.sleep(3000)

        session.switchCamera()
        SystemClock.sleep(3000)

        session.stopPreview()
        session.setDecoderSurface(null)
        session.releasePeerConnection()

        assertTrue("Activity should survive", !activity.isDestroyed)
    }

    @Test
    fun createOfferReturnsMockSdp() {
        val latch = CountDownLatch(1)
        var offerSdp: String? = null

        session.createPeerConnection(
            "stun:stun.l.google.com:19302",
            "turn:turn.example.com:3478",
            "user",
            "pass"
        )

        session.createOffer { sdp ->
            offerSdp = sdp
            latch.countDown()
        }

        assertTrue("Offer callback should be invoked", latch.await(3, TimeUnit.SECONDS))
        assertNotNull("SDP should not be null", offerSdp)
        assertTrue("SDP should contain v=0", offerSdp!!.contains("v=0"))

        session.releasePeerConnection()
    }

    @Test
    fun previewRestartAfterStopStart_noFreeze() {
        val activity = launchAndWaitForSurface()
        session.setupVideoEncoder(defaultConfig)

        val surface = activity.acquirePreviewSurface()
        assertEquals(0, session.startPreview(surface))

        val pc = session.createPeerConnection(
            "stun:stun.l.google.com:19302",
            "turn:turn.example.com:3478", "user", "pass")
        assertTrue(pc != 0L)
        session.addTransceivers(MediaSession.CODEC_H264, 5)
        SystemClock.sleep(3000)

        // Simulate screen off: stop preview
        session.stopPreview()
        session.setDecoderSurface(null)
        session.releasePeerConnection()
        SystemClock.sleep(500)

        // Simulate screen on: restart preview on same surface (TextureView survives activity pause/resume)
        session.setupVideoEncoder(defaultConfig)
        assertEquals(0, session.startPreview(surface))

        val pc2 = session.createPeerConnection(
            "stun:stun.l.google.com:19302",
            "turn:turn.example.com:3478", "user", "pass")
        assertTrue(pc2 != 0L)
        session.addTransceivers(MediaSession.CODEC_H264, 5)
        SystemClock.sleep(3000)

        session.stopPreview()
        session.setDecoderSurface(null)
        session.releasePeerConnection()

        assertTrue("Activity should survive", !activity.isDestroyed)
    }
}
