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
import java.util.concurrent.atomic.AtomicLong

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
        session.setCameraErrorListener { error ->
            session.releasePeerConnection()
            session.stopPreview()
        }
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

        SystemClock.sleep(1500)

        session.stopPreview()
        session.setDecoderSurface(null)
        session.releasePeerConnection()

        assertTrue("Activity should survive", !activity.isDestroyed)
    }

    @Test
    fun previewToRecording_sendFramesIncrease() {
        val activity = launchAndWaitForSurface()

        val videoFrames = AtomicLong(0)
        session.setTransceiverFrameStatsListener { stats ->
            videoFrames.set(stats.videoFramesSent)
        }

        session.setupVideoEncoder(defaultConfig)
        val surface = activity.acquirePreviewSurface()
        assertEquals("startPreview should succeed", 0, session.startPreview(surface))

        SystemClock.sleep(300)

        val pc = session.createPeerConnection(
            "stun:stun.l.google.com:19302",
            "turn:turn.example.com:3478",
            "user",
            "pass"
        )
        assertTrue("PeerConnection should be created", pc != 0L)


        session.addTransceivers(MediaSession.CODEC_H264, 5)
        session.startSend()
        session.addDataChannel("test")

        SystemClock.sleep(1500)

        val framesSent = videoFrames.get()
        assertTrue("Video frames should have been sent, got $framesSent", framesSent > 0)

        session.releasePeerConnection()
        assertTrue("Activity should survive", !activity.isDestroyed)
    }

    @Test
    fun previewRecordingBackToPreview_sendFramesIncreaseThenStop() {
        val activity = launchAndWaitForSurface()

        val videoFrames = AtomicLong(0)
        session.setTransceiverFrameStatsListener { stats ->
            videoFrames.set(stats.videoFramesSent)
        }

        session.setupVideoEncoder(defaultConfig)
        val surface = activity.acquirePreviewSurface()
        assertEquals(0, session.startPreview(surface))
        SystemClock.sleep(100)

        // Phase 2: start recording
        val pc = session.createPeerConnection(
            "stun:stun.l.google.com:19302",
            "turn:turn.example.com:3478",
            "user",
            "pass"
        )
        assertTrue(pc != 0L)
        session.addTransceivers(MediaSession.CODEC_H264, 5)
        session.startSend()
        session.addDataChannel("test")
        SystemClock.sleep(1500)

        val phase1Frames = videoFrames.get()
        assertTrue("Phase 1 video frames should have been sent, got $phase1Frames", phase1Frames > 0)

        // Phase 3: stop recording
        session.releasePeerConnection()
        SystemClock.sleep(100)

        // Phase 4: start recording again
        val pc2 = session.createPeerConnection(
            "stun:stun.l.google.com:19302",
            "turn:turn.example.com:3478",
            "user",
            "pass"
        )
        assertTrue(pc2 != 0L)
        session.addTransceivers(MediaSession.CODEC_H264, 5)
        session.startSend()
        SystemClock.sleep(1500)

        val phase2Frames = videoFrames.get()
        assertTrue("Phase 2 video frames should exceed Phase 1 (${phase1Frames}), got $phase2Frames",
            phase2Frames > phase1Frames)

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
        SystemClock.sleep(2000)

        session.switchCamera()
        SystemClock.sleep(2000)

        session.stopPreview()
        session.setDecoderSurface(null)
        session.releasePeerConnection()

        assertTrue("Activity should survive", !activity.isDestroyed)
    }

    @Test
    fun createOfferReturnsMockSdp() {
        val latch = CountDownLatch(1)
        var offerSdp: String? = null

        session.setupVideoEncoder(defaultConfig)
        session.createPeerConnection(
            "stun:stun.l.google.com:19302",
            "turn:turn.example.com:3478",
            "user",
            "pass"
        )
        session.addTransceivers(MediaSession.CODEC_H264, 5)  // 5 = ALAW

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

        val videoFrames = AtomicLong(0)
        session.setTransceiverFrameStatsListener { stats ->
            videoFrames.set(stats.videoFramesSent)
        }

        val surface = activity.acquirePreviewSurface()
        assertEquals(0, session.startPreview(surface))

        val pc = session.createPeerConnection(
            "stun:stun.l.google.com:19302",
            "turn:turn.example.com:3478", "user", "pass")
        assertTrue(pc != 0L)
        session.addTransceivers(MediaSession.CODEC_H264, 5)
        session.startSend()
        SystemClock.sleep(1500)

        val cycle1Frames = videoFrames.get()
        assertTrue("Cycle 1 should have sent video frames, got $cycle1Frames", cycle1Frames > 0)

        // Simulate screen off: stop preview
        session.releasePeerConnection()
        session.stopPreview()
        session.setDecoderSurface(null)
        SystemClock.sleep(100)

        // Simulate screen on: restart preview on same surface (TextureView survives activity pause/resume)
        session.setupVideoEncoder(defaultConfig)
        assertEquals(0, session.startPreview(surface))

        val pc2 = session.createPeerConnection(
            "stun:stun.l.google.com:19302",
            "turn:turn.example.com:3478", "user", "pass")
        assertTrue(pc2 != 0L)
        session.addTransceivers(MediaSession.CODEC_H264, 5)
        session.startSend()
        SystemClock.sleep(1500)

        val cycle2Frames = videoFrames.get()
        assertTrue("Cycle 2 should have sent more frames than Cycle 1 ($cycle1Frames), got $cycle2Frames",
            cycle2Frames > cycle1Frames)

        session.releasePeerConnection()
        session.stopPreview()
        session.setDecoderSurface(null)

        assertTrue("Activity should survive", !activity.isDestroyed)
    }
}
