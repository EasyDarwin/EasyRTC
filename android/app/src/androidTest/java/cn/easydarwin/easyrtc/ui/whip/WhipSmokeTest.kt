package cn.easydarwin.easyrtc.ui.whip

import android.Manifest
import android.os.SystemClock
import com.google.android.material.bottomnavigation.BottomNavigationView
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.rule.GrantPermissionRule
import cn.easydarwin.easyrtc.MainActivity
import cn.easydarwin.easyrtc.R
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class WhipSmokeTest {

    @get:Rule
    val permissionRule: GrantPermissionRule = GrantPermissionRule.grant(
        Manifest.permission.CAMERA,
        Manifest.permission.RECORD_AUDIO,
        Manifest.permission.MODIFY_AUDIO_SETTINGS
    )

    @get:Rule
    val activityRule = ActivityScenarioRule(MainActivity::class.java)

    @Test
    fun launchWhipAndStartDoesNotCrash() {
        activityRule.scenario.onActivity { activity ->
            val bottomNav = activity.findViewById<BottomNavigationView>(R.id.bottomNavigationView)
            bottomNav.selectedItemId = R.id.navigation_whip_push
        }

        SystemClock.sleep(1500)

        activityRule.scenario.onActivity { activity ->
            val fragment = requireNotNull(activity.supportFragmentManager.findFragmentByTag("whip_push")) {
                "WHIP fragment should be attached"
            }
            val root = requireNotNull(fragment.view) { "WHIP fragment view missing" }
            val startButton = root.findViewById<android.widget.Button>(R.id.btn_toggle_stream)
            startButton.performClick()
        }

        SystemClock.sleep(4000)

        activityRule.scenario.onActivity { activity ->
            val fragment = requireNotNull(activity.supportFragmentManager.findFragmentByTag("whip_push")) {
                "WHIP fragment should still be attached after start"
            }
            val statusView = fragment.requireView().findViewById<android.widget.TextView>(R.id.tv_status)
            val text = statusView.text?.toString().orEmpty()
            assertTrue("status should contain start marker, was: $text", text.contains("开始推流"))
        }
    }
}
