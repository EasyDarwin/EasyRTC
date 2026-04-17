package cn.easydarwin.easyrtc.ui.hub

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProvider
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import cn.easydarwin.easyrtc.R
import cn.easydarwin.easyrtc.core.result.AppResult
import cn.easydarwin.easyrtc.core.ui.UiState
import cn.easydarwin.easyrtc.domain.model.HubItem
import cn.easydarwin.easyrtc.domain.usecase.ChannelProvider
import cn.easydarwin.easyrtc.domain.usecase.DeviceProvider
import cn.easydarwin.easyrtc.domain.usecase.GetHubItemsUseCase
import com.google.android.material.button.MaterialButton

class HubFragment : Fragment() {

    private lateinit var viewModel: HubViewModel
    private var recyclerView: RecyclerView? = null
    private var loadingView: View? = null
    private var stateText: TextView? = null
    private var retryButton: MaterialButton? = null
    private val hubAdapter = HubStatusAdapter()

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return inflater.inflate(R.layout.fragment_hub, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        viewModel = ViewModelProvider(this, HubViewModelFactory(createUseCase()))[HubViewModel::class.java]

        recyclerView = view.findViewById<RecyclerView>(R.id.recyclerHub).also {
            it.layoutManager = LinearLayoutManager(requireContext())
            it.adapter = hubAdapter
        }
        loadingView = view.findViewById(R.id.progressHub)
        stateText = view.findViewById(R.id.textHubState)
        retryButton = view.findViewById<MaterialButton>(R.id.buttonHubRetry).also {
            it.setOnClickListener { viewModel.load() }
        }

        viewModel.uiState.observe(viewLifecycleOwner, ::renderState)
        viewModel.load()
    }

    override fun onDestroyView() {
        recyclerView?.adapter = null
        recyclerView = null
        loadingView = null
        stateText = null
        retryButton = null
        super.onDestroyView()
    }

    private fun renderState(state: UiState<List<HubItem>>) {
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
                hubAdapter.submitList(state.data)
            }

            is UiState.Empty -> {
                loadingView.isVisible = false
                recyclerView.isVisible = false
                stateText.isVisible = true
                stateText.text = state.reason
                retryButton.isVisible = true
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

    private fun createUseCase(): GetHubItemsUseCase {
        return GetHubItemsUseCase(
            deviceProvider = DeviceProvider { AppResult.Success(sampleDevices()) },
            channelProvider = ChannelProvider { AppResult.Success(sampleChannels()) }
        )
    }

    private fun sampleDevices(): List<HubItem.Device> {
        return listOf(
            HubItem.Device(
                id = "device-1",
                name = "Lobby Camera",
                isOnline = true,
                ip = "192.168.1.20",
                channelCount = 2,
                model = "ER-Edge-200",
                manufacturer = "EasyDarwin"
            ),
            HubItem.Device(
                id = "device-2",
                name = "Parking Entrance",
                isOnline = false,
                ip = "192.168.1.36",
                channelCount = 1,
                model = "ER-Edge-100",
                manufacturer = "EasyDarwin"
            )
        )
    }

    private fun sampleChannels(): List<HubItem.Channel> {
        return listOf(
            HubItem.Channel(
                id = "channel-1",
                name = "CH-101",
                isOnline = true,
                transport = "TCP",
                createdAt = "2026-03-28 12:31",
                deviceId = "device-1"
            ),
            HubItem.Channel(
                id = "channel-2",
                name = "CH-201",
                isOnline = false,
                transport = "UDP",
                createdAt = "2026-03-27 09:20",
                deviceId = "device-2"
            )
        )
    }
}

private class HubStatusAdapter : ListAdapter<HubItem, HubStatusAdapter.HubStatusViewHolder>(DIFF_CALLBACK) {

    private companion object {
        val DIFF_CALLBACK = object : DiffUtil.ItemCallback<HubItem>() {
            override fun areItemsTheSame(oldItem: HubItem, newItem: HubItem): Boolean {
                return oldItem.id == newItem.id && oldItem::class == newItem::class
            }

            override fun areContentsTheSame(oldItem: HubItem, newItem: HubItem): Boolean {
                return oldItem == newItem
            }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): HubStatusViewHolder {
        val itemView = LayoutInflater.from(parent.context).inflate(R.layout.item_hub_status_card, parent, false)
        return HubStatusViewHolder(itemView)
    }

    override fun onBindViewHolder(holder: HubStatusViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    class HubStatusViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {

        private val typeView: TextView = itemView.findViewById(R.id.tvHubItemType)
        private val nameView: TextView = itemView.findViewById(R.id.tvHubItemName)
        private val statusView: TextView = itemView.findViewById(R.id.tvHubItemStatus)
        private val subtitleView: TextView = itemView.findViewById(R.id.tvHubItemSubtitle)
        private val metaView: TextView = itemView.findViewById(R.id.tvHubItemMeta)

        fun bind(item: HubItem) {
            val context = itemView.context
            nameView.text = item.name
            statusView.text = if (item.isOnline) context.getString(R.string.hub_status_online) else context.getString(R.string.hub_status_offline)
            statusView.setTextColor(
                context.getColor(
                    if (item.isOnline) android.R.color.holo_green_dark else android.R.color.darker_gray
                )
            )

            when (item) {
                is HubItem.Device -> {
                    typeView.text = context.getString(R.string.hub_type_device)
                    subtitleView.text = context.getString(R.string.hub_device_subtitle, item.ip, item.channelCount)
                    metaView.text = context.getString(R.string.hub_device_meta, item.manufacturer, item.model)
                }

                is HubItem.Channel -> {
                    typeView.text = context.getString(R.string.hub_type_channel)
                    subtitleView.text = context.getString(R.string.hub_channel_subtitle, item.deviceId, item.transport)
                    metaView.text = context.getString(R.string.hub_channel_meta, item.createdAt)
                }
            }
        }
    }
}

private class HubViewModelFactory(
    private val getHubItemsUseCase: GetHubItemsUseCase
) : ViewModelProvider.Factory {
    @Suppress("UNCHECKED_CAST")
    override fun <T : androidx.lifecycle.ViewModel> create(modelClass: Class<T>): T {
        if (modelClass.isAssignableFrom(HubViewModel::class.java)) {
            return HubViewModel(getHubItemsUseCase) as T
        }
        throw IllegalArgumentException("Unknown ViewModel class: ${modelClass.name}")
    }
}
