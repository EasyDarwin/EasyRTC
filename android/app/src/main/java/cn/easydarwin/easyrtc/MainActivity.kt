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
import android.view.WindowManager
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.Observer
import androidx.constraintlayout.widget.ConstraintSet
import com.tencent.bugly.crashreport.CrashReport
import cn.easydarwin.easyrtc.service.WebSocketService
import cn.easydarwin.easyrtc.ui.live.LiveFragment
import cn.easydarwin.easyrtc.ui.hub.EmptyTabFragment
import cn.easydarwin.easyrtc.fragment.SettingFragment
import cn.easydarwin.easyrtc.utils.AppLogStore
import cn.easydarwin.easyrtc.utils.SPUtil
import cn.easyrtc.helper.MagicFileHelper
import com.google.android.material.bottomnavigation.BottomNavigationView

class MainActivity : AppCompatActivity() {

    var bottomNavigationView: BottomNavigationView? = null
    val webSocketServiceLiveData = MutableLiveData<WebSocketService?>()
    val incomingCallLiveData = MutableLiveData<WebSocketService.Event.IncomingCall>()
    private var webSocketServiceConnection: ServiceConnection? = null
    var currentFragmentTag: String? = null

    var ws: WebSocketService? = null
    private var mainRoot: androidx.constraintlayout.widget.ConstraintLayout? = null
    val observer = Observer<WebSocketService.Event> { event ->
        if (event is WebSocketService.Event.IncomingCall) {
            incomingCallLiveData.postValue(event)
            runOnUiThread {
                if (bottomNavigationView?.selectedItemId != R.id.navigation_p2p_call) {
                    bottomNavigationView?.selectedItemId = R.id.navigation_p2p_call
                }
            }
        }
    }

    var cFragmentTag: String?
        get() = currentFragmentTag
        set(value) {
            currentFragmentTag = value
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        setTheme(R.style.Theme_EasyRTCDevice_Device)
        super.onCreate(savedInstanceState)

        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.statusBarColor = Color.WHITE
        WindowCompat.getInsetsController(window, window.decorView).isAppearanceLightStatusBars = true

        SPUtil.init(this)
        AppLogStore.init(this)
        MagicFileHelper.init(this)

        setContentView(R.layout.activity_main)
        mainRoot = findViewById(R.id.main_root)

        CrashReport.initCrashReport(applicationContext, "5aece7ce65", false)

        checkPermission()
        bindWebSocketService()

        initBNView()
        supportFragmentManager.addOnBackStackChangedListener { syncChromeVisibility() }
        syncChromeVisibility()
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

    private fun checkPermission() {
        if (!allPermissionsGranted()) {
            ActivityCompat.requestPermissions(this, REQUIRED_PERMISSIONS, REQUEST_CODE_PERMISSIONS)
        }
    }

    private fun initBNView() {
        if (bottomNavigationView != null) return

        bottomNavigationView = findViewById(R.id.bottomNavigationView)

        bottomNavigationView?.setOnItemSelectedListener {
            when (it.itemId) {
                R.id.navigation_p2p_call -> {
                    switchFragment("p2p_call")
                    true
                }

                R.id.navigation_whip_push -> {
                    switchFragment("whip_push")
                    true
                }

                R.id.navigation_ip_direct -> {
                    switchFragment("ip_direct")
                    true
                }

                else -> false
            }
        }

        bottomNavigationView?.selectedItemId = R.id.navigation_p2p_call
    }

    fun switchFragment(tag: String, commitNow: Boolean = false) {
        val fm = supportFragmentManager
        if (fm.isStateSaved) return

        val transaction = fm.beginTransaction()
        try {

            currentFragmentTag = tag

            fm.fragments.forEach { transaction.remove(it) }

            val exist = fm.findFragmentByTag(tag)
            if (exist != null) {
                transaction.show(exist)
            } else {
                val fragment = when (tag) {
                    "p2p_call" -> LiveFragment.newInstance()
                    "whip_push" -> EmptyTabFragment()
                    "ip_direct" -> EmptyTabFragment()
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
        bottomNavigationView?.visibility = if (fullScreen) android.view.View.GONE else android.view.View.VISIBLE

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

                // Notify live fragment to reinitialize camera after permission grant
                supportFragmentManager.fragments.forEach {
                    if (it.tag == "p2p_call") {
                        LiveFragment.notifyPermissionGranted(it)
                    }
                }

            } else {
                Toast.makeText(this, "Permissions not granted!", Toast.LENGTH_SHORT).show()
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
