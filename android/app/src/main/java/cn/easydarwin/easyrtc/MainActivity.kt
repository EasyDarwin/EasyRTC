package cn.easydarwin.easyrtc

import android.Manifest
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.graphics.Color
import android.os.IBinder
import android.os.Bundle
import androidx.core.view.WindowCompat
import android.view.View
import android.view.WindowManager
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.Observer
import androidx.constraintlayout.widget.ConstraintSet
import com.tencent.bugly.crashreport.CrashReport
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import cn.easydarwin.easyrtc.service.WebSocketService
import cn.easydarwin.easyrtc.ui.live.LiveFragment
import cn.easydarwin.easyrtc.ui.whip.WhipFragment
import cn.easydarwin.easyrtc.ui.ipdirect.IpDirectContainerFragment
import cn.easydarwin.easyrtc.fragment.SettingFragment
import cn.easydarwin.easyrtc.utils.AppLogStore
import cn.easydarwin.easyrtc.utils.AppUpdateChecker
import cn.easydarwin.easyrtc.utils.SPUtil
import cn.easyrtc.EasyRTCLog
import cn.easyrtc.helper.MagicFileHelper
import com.google.android.material.bottomnavigation.BottomNavigationView

class MainActivity : AppCompatActivity() {

    lateinit var bottomNavigationView: BottomNavigationView
    val webSocketServiceLiveData = MutableLiveData<WebSocketService?>()
    val incomingCallLiveData = MutableLiveData<WebSocketService.Event.IncomingCall>()
    private var webSocketServiceConnection: ServiceConnection? = null
    private val mainScope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
    var currentFragmentTag: String? = null

    var ws: WebSocketService? = null
    private var mainRoot: androidx.constraintlayout.widget.ConstraintLayout? = null
    val observer = Observer<WebSocketService.Event> { event ->
        if (event is WebSocketService.Event.IncomingCall) {
            incomingCallLiveData.postValue(event)
            runOnUiThread {
                if (bottomNavigationView.selectedItemId != R.id.navigation_p2p_call) {
                    bottomNavigationView.selectedItemId = R.id.navigation_p2p_call
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        setTheme(R.style.Theme_EasyRTCDevice_Device)
        SPUtil.init(this)
        super.onCreate(savedInstanceState)

        val permissionsGranted = allPermissionsGranted()
        if (!permissionsGranted) {
            val fm = supportFragmentManager
            if (fm.fragments.isNotEmpty()) {
                fm.beginTransaction().apply {
                    fm.fragments.forEach { remove(it) }
                    commitNow()
                }
            }
        }

        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.statusBarColor = Color.WHITE
        WindowCompat.getInsetsController(window, window.decorView).isAppearanceLightStatusBars = true

        AppLogStore.init(this)
        EasyRTCLog.setSink { level, tag, message, throwable ->
            val logLine = "[${level.name}][$tag] $message"
            if (throwable != null) {
                AppLogStore.appendCritical(tag, logLine, throwable)
            } else {
                AppLogStore.appendTimestamped(logLine)
            }
        }

        AppLogStore.appendCritical(
            tag = "MainActivity",
            message = "Application bootstrapped, pid=${android.os.Process.myPid()}, deviceId=${SPUtil.getInstance().rtcUserUUID}, permissionsGranted=$permissionsGranted"
        )
        MagicFileHelper.init(this)

        setContentView(R.layout.activity_main)
        mainRoot = findViewById(R.id.main_root)

        CrashReport.initCrashReport(applicationContext, "5aece7ce65", false)
        mainScope.launch { AppUpdateChecker.check(this@MainActivity) }

        bindWebSocketService()

        bottomNavigationView = findViewById(R.id.bottomNavigationView)
        bottomNavigationView.setOnItemSelectedListener {
            when (it.itemId) {
                R.id.navigation_p2p_call -> {
                    switchFragment("p2p_call"); true
                }
                R.id.navigation_whip_push -> {
                    switchFragment("whip_push"); true
                }
                R.id.navigation_ip_direct -> {
                    switchFragment("ip_direct"); true
                }
                else -> false
            }
        }

        if (permissionsGranted) {
            if (supportFragmentManager.fragments.isEmpty()) {
                switchFragment("p2p_call")
            }
        } else {
            bottomNavigationView.visibility = View.GONE
            ActivityCompat.requestPermissions(this, REQUIRED_PERMISSIONS, REQUEST_CODE_PERMISSIONS)
        }
        supportFragmentManager.addOnBackStackChangedListener { syncChromeVisibility() }
        syncChromeVisibility()
    }

    override fun onResume() {
        super.onResume()
        if (BuildConfig.FLAVOR == "mock") {
            Toast.makeText(this,"MOCK!!!!", Toast.LENGTH_LONG).show();
        }
    }

    private fun bindWebSocketService() {
        if (webSocketServiceConnection != null) return
        val connection = object : ServiceConnection {

            override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
                val binder = service as? WebSocketService.LocalBinder
                ws = binder?.getService()
                webSocketServiceLiveData.postValue(ws)
                ws?.events?.observeForever(observer)
            }

            override fun onServiceDisconnected(name: ComponentName?) {
                ws?.events?.removeObserver(observer)
                webSocketServiceLiveData.postValue(null)
            }
        }
        webSocketServiceConnection = connection
        bindService(Intent(this, WebSocketService::class.java), connection, Context.BIND_AUTO_CREATE)
    }

    fun switchFragment(tag: String, commitNow: Boolean = false) {
        val fm = supportFragmentManager
        if (fm.isStateSaved) return
        val exist = fm.findFragmentByTag(tag)
        if (exist != null && exist.isVisible) return

        val transaction = fm.beginTransaction()
        try {
            currentFragmentTag = tag

            fm.fragments.forEach { transaction.remove(it) }

            if (exist != null) {
                transaction.show(exist)
            } else {
                val fragment = when (tag) {
                    "p2p_call" -> LiveFragment.newInstance()
                    "whip_push" -> WhipFragment.newInstance()
                    "ip_direct" -> IpDirectContainerFragment.newInstance()
                    else -> return
                }
                transaction.add(R.id.fragment_container, fragment, tag)
            }

        }finally {
            if (commitNow) transaction.commitNow()
            else transaction.commit()
        }
    }

    fun openSettingsScreen() {
        val fm = supportFragmentManager
        if (fm.isStateSaved) return

        fm.beginTransaction()
            .replace(R.id.fragment_container, SettingFragment())
            .addToBackStack("settings")
            .commit()
        syncChromeVisibility()
    }

    private fun syncChromeVisibility() {
        val fullScreen = supportFragmentManager.backStackEntryCount > 0
        bottomNavigationView.visibility = if (fullScreen) android.view.View.GONE else android.view.View.VISIBLE

        val root = mainRoot ?: return
        val set = ConstraintSet().apply { clone(root) }
        if (fullScreen) {
            set.connect(R.id.fragment_container, ConstraintSet.BOTTOM, ConstraintSet.PARENT_ID, ConstraintSet.BOTTOM)
        } else {
            set.connect(R.id.fragment_container, ConstraintSet.BOTTOM, R.id.bottomNavigationView, ConstraintSet.TOP)
        }
        set.applyTo(root)
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)

        if (requestCode == REQUEST_CODE_PERMISSIONS) {
            if (allPermissionsGranted()) {
                AppLogStore.appendCritical(tag = "MainActivity", message = "Permissions granted via onRequestPermissionsResult")
                bottomNavigationView.visibility = View.VISIBLE
                switchFragment("p2p_call")
            } else {
                AppLogStore.appendCritical(tag = "MainActivity", message = "Permissions denied, finishing activity")
                Toast.makeText(this, "Permissions not granted!", Toast.LENGTH_SHORT).show()
                finish()
            }
        }
    }

    private fun allPermissionsGranted() = REQUIRED_PERMISSIONS.all {
        ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
    }

    companion object {
        private const val REQUEST_CODE_PERMISSIONS = 10

        private val REQUIRED_PERMISSIONS = arrayOf(
            Manifest.permission.CAMERA,
            Manifest.permission.RECORD_AUDIO,
            Manifest.permission.MODIFY_AUDIO_SETTINGS
        )
    }

    override fun onDestroy() {
        ws?.events?.removeObserver(observer)
        webSocketServiceConnection?.let { unbindService(it) }
        webSocketServiceConnection = null
        super.onDestroy()
    }
}
