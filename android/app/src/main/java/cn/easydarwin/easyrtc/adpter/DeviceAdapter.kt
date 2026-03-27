package cn.easydarwin.easyrtc.adpter

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import cn.easydarwin.easyrtc.R

class DeviceAdapter : RecyclerView.Adapter<DeviceAdapter.DeviceViewHolder>() {

    private val deviceList = mutableListOf<Device>()

    // 设备数据类
    data class Device(
        val id: String,
        val name: String,
        val status: Boolean, // online, offline
        val ip: String,
        val addr: String,
        val channelNum: Int,
        val model: String,
        val manufacturer: String,
    )

    class DeviceViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        //        private val ivDeviceIcon: ImageView = itemView.findViewById(R.id.ivDeviceIcon)
        private val tvDeviceName: TextView = itemView.findViewById(R.id.tvDeviceName)
        private val tvDeviceStatus: TextView = itemView.findViewById(R.id.tvDeviceStatus)
        private val tvIP: TextView = itemView.findViewById(R.id.tvLastOnline)
        private val tvChannelCount: TextView = itemView.findViewById(R.id.tvChannelCount)
        private val tvModel: TextView = itemView.findViewById(R.id.tvModel)
        private val tvManufacturer: TextView = itemView.findViewById(R.id.tvManufacturer)

        fun bind(device: Device, listener: OnItemClickListener?) {
            tvDeviceName.text = device.name
            tvDeviceStatus.text = if (device.status) "在线" else "离线"
            tvIP.text = device.ip
            tvChannelCount.text = "通道数 ${device.channelNum}"
            tvModel.text = "型号 ${device.model}"
            tvManufacturer.text = "厂商 ${device.manufacturer}"


            if (device.status) tvDeviceStatus.setTextColor(itemView.context.getColor(android.R.color.holo_green_dark))
            else tvDeviceStatus.setTextColor(itemView.context.getColor(android.R.color.darker_gray))


            itemView.setOnClickListener {
                listener?.onItemClick(device)
            }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): DeviceViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(R.layout.item_device_card, parent, false)
        return DeviceViewHolder(view)
    }

    override fun onBindViewHolder(holder: DeviceViewHolder, position: Int) {
        holder.bind(deviceList[position], itemClickListener)
    }

    override fun getItemCount(): Int = deviceList.size

    // 添加新数据（用于加载更多）
    fun addDevices(newDevices: List<Device>) {
        val startPosition = deviceList.size
        deviceList.addAll(newDevices)
        notifyItemRangeInserted(startPosition, newDevices.size)
    }

    // 设置数据（用于刷新）
    fun setDevices(devices: List<Device>) {
        deviceList.clear()
        deviceList.addAll(devices)
        notifyDataSetChanged()
    }

    fun getTotal(): Int {
        return deviceList.size
    }


    interface OnItemClickListener {
        fun onItemClick(device: Device)
    }

    private var itemClickListener: OnItemClickListener? = null

    fun setOnItemClickListener(listener: OnItemClickListener) {
        this.itemClickListener = listener
    }
}