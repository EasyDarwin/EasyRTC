package cn.easydarwin.easyrtc

import android.content.Intent
import android.view.Surface
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

@RunWith(AndroidJUnit4::class)
class DecoderPlaybackTest {

    @get:Rule
    val activityRule = ActivityTestRule(DecoderPlaybackActivity::class.java, false, false)

    private external fun nativeReplayFrames(filePath: String, surface: Surface, codec: Int)

    companion object {
        init {
            System.loadLibrary("easyrtc_media")
        }
    }

    @Test
    fun replayDumpedVideo() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val dumpFile = File(context.getExternalFilesDir(null), "easyrtc_av.bin")
        if (!dumpFile.exists()) {
            throw IllegalStateException("Dump file not found at ${dumpFile.absolutePath}. Push with: adb push easyrtc_av.bin ${context.getExternalFilesDir(null)?.absolutePath}/")
        }

        val intent = Intent(InstrumentationRegistry.getInstrumentation().targetContext, DecoderPlaybackActivity::class.java)
        val activity = activityRule.launchActivity(intent)

        activity.surfaceLatch.await()
        val surface = activity.surfaceHolder?.surface
            ?: throw IllegalStateException("Surface not available")

        nativeReplayFrames(dumpFile.absolutePath, surface, 1)

        Thread.sleep(1000)
    }
}
