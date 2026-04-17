package cn.easydarwin.easyrtc.ui.live

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import cn.easydarwin.easyrtc.fragment.HomeFragment
import cn.easydarwin.easyrtc.R
import androidx.fragment.app.Fragment

class LiveFragment : Fragment() {
    private var containerId: Int = R.id.live_home_container

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return FrameLayout(requireContext()).apply {
            id = containerId
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        if (childFragmentManager.findFragmentByTag(HOME_FRAGMENT_TAG) == null) {
            childFragmentManager.beginTransaction()
                .replace(containerId, HomeFragment(), HOME_FRAGMENT_TAG)
                .commitNow()
        }
    }

    fun onPermissionGranted() {
        (childFragmentManager.findFragmentByTag(HOME_FRAGMENT_TAG) as? HomeFragment)?.onPermissionGranted()
    }

    companion object {
        private const val HOME_FRAGMENT_TAG = "live_home"

        fun newInstance(): LiveFragment = LiveFragment()

        fun notifyPermissionGranted(fragment: Fragment) {
            (fragment as? LiveFragment)?.onPermissionGranted()
        }
    }
}
