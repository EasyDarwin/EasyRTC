package cn.easydarwin.easyrtc

import android.content.Context
import android.content.Intent
import android.content.res.AssetManager
import android.os.Process
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
        Process.setThreadPriority(Process.myTid(), -1)
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val dumpFile = File(context.getExternalFilesDir(null), "easyrtc_av.bin")
        if (!dumpFile.exists()) {
            
            // we need to use the test context to access the assets, and the target context to write the file, so we get both contexts here
            val testContext = InstrumentationRegistry.getInstrumentation().context
//            let's copy the assets/easyrtc_av.bin to the external files dir for testing
            val assetManager: AssetManager = testContext.assets
            assetManager.open("easyrtc_av.bin").use { inputStream ->
                dumpFile.outputStream().use { outputStream ->
                    inputStream.copyTo(outputStream)
                }
            }
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
