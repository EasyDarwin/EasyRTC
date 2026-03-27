package cn.easydarwin.easyrtc

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.WindowManager
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import cn.easydarwin.easyrtc.fragment.AboutFragment
import cn.easydarwin.easyrtc.fragment.HomeFragment
import cn.easydarwin.easyrtc.fragment.SettingFragment
import cn.easydarwin.easyrtc.utils.SPUtil
import cn.easyrtc.helper.MagicFileHelper
import com.google.android.material.bottomnavigation.BottomNavigationView

class MainActivity : AppCompatActivity() {

    private var mBNView: BottomNavigationView? = null
    private var cFragment: Fragment? = null
    var cFragmentTag: String? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        setTheme(R.style.Theme_EasyRTCDevice_Login)
        super.onCreate(savedInstanceState)

        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        SPUtil.init(this)
        MagicFileHelper.init(this)

        setContentView(R.layout.activity_main)

        checkPermission()

        initBNView()
    }

    private fun checkPermission() {
        if (!allPermissionsGranted()) {
            ActivityCompat.requestPermissions(this, REQUIRED_PERMISSIONS, REQUEST_CODE_PERMISSIONS)
        }
    }

    private fun initBNView() {
        if (mBNView != null) return

        mBNView = findViewById(R.id.bottomNavigationView)

        mBNView?.setOnItemSelectedListener {
            when (it.itemId) {
                R.id.navigation_home -> {
                    switchFragment(HomeFragment(), "home")
                    true
                }

                R.id.navigation_setting -> {
                    switchFragment(SettingFragment(), "setting")
                    true
                }

                R.id.navigation_about -> {
                    switchFragment(AboutFragment(), "about")
                    true
                }

                else -> false
            }
        }

        switchFragment(HomeFragment(), "home")
    }

    private fun switchFragment(fragment: Fragment, tag: String) {
        val fm = supportFragmentManager
        val transaction = fm.beginTransaction()
        cFragmentTag = tag

        fm.fragments.forEach { transaction.hide(it) }

        val exist = fm.findFragmentByTag(tag)
        if (exist != null) {
            transaction.show(exist)
            cFragment = exist
        } else {
            transaction.add(R.id.fragment_container, fragment, tag)
            cFragment = fragment
        }

        transaction.commitAllowingStateLoss()
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)

        if (requestCode == REQUEST_CODE_PERMISSIONS) {
            if (allPermissionsGranted()) {

                // ✅ 通知 HomeFragment 重新初始化 Camera
                supportFragmentManager.fragments.forEach {
                    if (it is HomeFragment) {
                        it.onPermissionGranted()
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
}