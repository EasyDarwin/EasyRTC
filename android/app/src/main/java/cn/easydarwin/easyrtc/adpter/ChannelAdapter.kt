package cn.easydarwin.easyrtc.adpter

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import cn.easydarwin.easyrtc.R

class ChannelAdapter : RecyclerView.Adapter<ChannelAdapter.ChannelViewHolder>() {

    private val channelList = mutableListOf<Channel>()

    // 设备数据类
    data class Channel(
        val id: String,
        val name: String,
        val status: Boolean, // online, offline
        val ip: String,
        val addr: String,
        val channelNum: Int,
        val model: String,
        val transport: String,
        val bid: String,
        val device_id: String,
        val created_at: String,
    )




    class ChannelViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        //        private val ivDeviceIcon: ImageView = itemView.findViewById(R.id.ivDeviceIcon)
        private val tvChannelName: TextView = itemView.findViewById(R.id.tvChannelName)
        private val tvChannelStatus: TextView = itemView.findViewById(R.id.tvChannelStatus)
        private val tvCreatedAt: TextView = itemView.findViewById(R.id.tvCreatedAt)
        private val tvTransport: TextView = itemView.findViewById(R.id.tvTransport)

        fun bind(device: Channel, listener: OnItemClickListener?) {
            tvChannelName.text = device.bid
            tvChannelStatus.text = if (device.status) "在线" else "离线"
            tvCreatedAt.text = "创建时间  ${device.created_at}"
            tvTransport.text = "媒体传输 ${device.transport}"


            if (device.status) tvChannelStatus.setTextColor(itemView.context.getColor(android.R.color.holo_green_dark))
            else tvChannelStatus.setTextColor(itemView.context.getColor(android.R.color.darker_gray))


            itemView.setOnClickListener {
                listener?.onItemClick(device)
            }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ChannelViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(R.layout.item_channel_card, parent, false)
        return ChannelViewHolder(view)
    }

    override fun onBindViewHolder(holder: ChannelViewHolder, position: Int) {
        holder.bind(channelList[position], itemClickListener)
    }

    override fun getItemCount(): Int = channelList.size

    // 添加新数据（用于加载更多）
    fun addDevices(newDevices: List<Channel>) {
        val startPosition = channelList.size
        channelList.addAll(newDevices)
        notifyItemRangeInserted(startPosition, newDevices.size)
    }

    // 设置数据（用于刷新）
    fun setDevices(devices: List<Channel>) {
        channelList.clear()
        channelList.addAll(devices)
        notifyDataSetChanged()
    }

    fun getTotal(): Int {
        return channelList.size
    }


    interface OnItemClickListener {
        fun onItemClick(device: Channel)
    }

    private var itemClickListener: OnItemClickListener? = null

    fun setOnItemClickListener(listener: OnItemClickListener) {
        this.itemClickListener = listener
    }
}