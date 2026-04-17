package cn.easydarwin.easyrtc.domain.model

sealed interface HubItem {
    val id: String
    val name: String
    val isOnline: Boolean

    data class Device(
        override val id: String,
        override val name: String,
        override val isOnline: Boolean,
        val ip: String,
        val channelCount: Int,
        val model: String,
        val manufacturer: String
    ) : HubItem

    data class Channel(
        override val id: String,
        override val name: String,
        override val isOnline: Boolean,
        val transport: String,
        val createdAt: String,
        val deviceId: String
    ) : HubItem
}
