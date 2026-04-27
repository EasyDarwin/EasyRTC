package cn.easydarwin.easyrtc.ui.hub

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import android.widget.Toast
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProvider
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import cn.easydarwin.easyrtc.MainActivity
import cn.easydarwin.easyrtc.R
import cn.easydarwin.easyrtc.core.ui.UiState
import cn.easydarwin.easyrtc.service.WebSocketService
import cn.easyrtc.EasyRTCUser
import com.google.android.material.button.MaterialButton

class HubFragment : Fragment() {

    private lateinit var viewModel: HubViewModel
    private var recyclerView: RecyclerView? = null
    private var loadingView: View? = null
    private var stateText: TextView? = null
    private var retryButton: MaterialButton? = null
    private val userAdapter = UserCardAdapter { user ->
        val activity = activity as? MainActivity ?: return@UserCardAdapter
        activity.ws?.call(user.uuid)
        Toast.makeText(requireContext(), "正在连接 ${user.username}", Toast.LENGTH_LONG).show()
        activity.bottomNavigationView?.selectedItemId = R.id.navigation_live
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return inflater.inflate(R.layout.fragment_hub, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        viewModel = ViewModelProvider(this)[HubViewModel::class.java]

        recyclerView = view.findViewById<RecyclerView>(R.id.recyclerHub).also {
            it.layoutManager = LinearLayoutManager(requireContext())
            it.adapter = userAdapter
        }
        loadingView = view.findViewById(R.id.progressHub)
        stateText = view.findViewById(R.id.textHubState)
        retryButton = view.findViewById<MaterialButton>(R.id.buttonHubRetry).also {
            it.setOnClickListener { viewModel.setLoading() }
        }

        viewModel.uiState.observe(viewLifecycleOwner, ::renderState)

        (activity as? MainActivity)?.webSocketServiceLiveData?.observe(viewLifecycleOwner) { service ->
            service?.events?.observe(viewLifecycleOwner) { event ->
                if (event is WebSocketService.Event.OnlineUsers) {
                    viewModel.updateUsers(event.users)
                }
            }
        }
    }

    override fun onDestroyView() {
        recyclerView?.adapter = null
        recyclerView = null
        loadingView = null
        stateText = null
        retryButton = null
        super.onDestroyView()
    }

    private fun renderState(state: UiState<List<EasyRTCUser>>) {
        val loadingView = loadingView ?: return
        val recyclerView = recyclerView ?: return
        val stateText = stateText ?: return
        val retryButton = retryButton ?: return

        when (state) {
            is UiState.Loading -> {
                loadingView.isVisible = true
                recyclerView.isVisible = false
                stateText.isVisible = true
                stateText.text = getString(R.string.hub_loading)
                retryButton.isVisible = false
            }

            is UiState.Content -> {
                loadingView.isVisible = false
                recyclerView.isVisible = true
                stateText.isVisible = false
                retryButton.isVisible = false
                userAdapter.submitList(state.data)
            }

            is UiState.Empty -> {
                loadingView.isVisible = false
                recyclerView.isVisible = false
                stateText.isVisible = true
                stateText.text = state.reason
                retryButton.isVisible = false
            }

            is UiState.Error -> {
                loadingView.isVisible = false
                recyclerView.isVisible = false
                stateText.isVisible = true
                stateText.text = state.message
                retryButton.isVisible = true
            }
        }
    }
}

private class UserCardAdapter(
    private val onCallClick: (EasyRTCUser) -> Unit
) : ListAdapter<EasyRTCUser, UserCardAdapter.UserCardViewHolder>(DIFF_CALLBACK) {

    private companion object {
        val DIFF_CALLBACK = object : DiffUtil.ItemCallback<EasyRTCUser>() {
            override fun areItemsTheSame(oldItem: EasyRTCUser, newItem: EasyRTCUser): Boolean {
                return oldItem.uuid == newItem.uuid
            }

            override fun areContentsTheSame(oldItem: EasyRTCUser, newItem: EasyRTCUser): Boolean {
                return oldItem == newItem
            }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): UserCardViewHolder {
        val itemView = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_hub_status_card, parent, false)
        return UserCardViewHolder(itemView, onCallClick)
    }

    override fun onBindViewHolder(holder: UserCardViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    class UserCardViewHolder(
        itemView: View,
        private val onCallClick: (EasyRTCUser) -> Unit
    ) : RecyclerView.ViewHolder(itemView) {

        private val typeView: TextView = itemView.findViewById(R.id.tvHubItemType)
        private val nameView: TextView = itemView.findViewById(R.id.tvHubItemName)
        private val statusView: TextView = itemView.findViewById(R.id.tvHubItemStatus)
        private val subtitleView: TextView = itemView.findViewById(R.id.tvHubItemSubtitle)
        private val callButton: View = itemView.findViewById(R.id.buttonCall)

        fun bind(user: EasyRTCUser) {
            val context = itemView.context
            typeView.text = context.getString(R.string.hub_type_user)
            nameView.text = user.username
            statusView.text = context.getString(R.string.hub_status_online)
            statusView.setTextColor(context.getColor(android.R.color.holo_green_dark))
            subtitleView.text = user.uuid

            callButton.setOnClickListener { onCallClick(user) }
            itemView.setOnClickListener { onCallClick(user) }
        }
    }
}
