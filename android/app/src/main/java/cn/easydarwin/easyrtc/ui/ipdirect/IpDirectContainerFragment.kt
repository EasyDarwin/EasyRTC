package cn.easydarwin.easyrtc.ui.ipdirect

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.fragment.app.Fragment
import cn.easydarwin.easyrtc.R

class IpDirectContainerFragment : Fragment() {

    private var containerId: Int = R.id.ip_direct_home_container

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
        if (childFragmentManager.findFragmentByTag(FRAGMENT_TAG) == null) {
            childFragmentManager.beginTransaction()
                .replace(containerId, IpDirectFragment(), FRAGMENT_TAG)
                .commitNow()
        }
    }

    companion object {
        private const val FRAGMENT_TAG = "ip_direct_home"

        fun newInstance(): IpDirectContainerFragment = IpDirectContainerFragment()
    }
}
