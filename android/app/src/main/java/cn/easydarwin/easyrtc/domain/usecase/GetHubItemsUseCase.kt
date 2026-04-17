package cn.easydarwin.easyrtc.domain.usecase

import cn.easydarwin.easyrtc.core.result.AppResult
import cn.easydarwin.easyrtc.domain.model.HubItem
import java.util.Locale

class GetHubItemsUseCase(
    private val deviceProvider: DeviceProvider,
    private val channelProvider: ChannelProvider
) {

    fun execute(): AppResult<List<HubItem>> {
        val devicesResult = deviceProvider.getDevices()
        if (devicesResult is AppResult.Error) {
            return devicesResult
        }

        val channelsResult = channelProvider.getChannels()
        if (channelsResult is AppResult.Error) {
            return channelsResult
        }

        val devices = (devicesResult as AppResult.Success).data
        val channels = (channelsResult as AppResult.Success).data

        val merged = mutableListOf<HubItem>()
        merged.addAll(devices)
        merged.addAll(channels)

        val sorted = merged.sortedWith(
                compareBy<HubItem> { if (it is HubItem.Device) 0 else 1 }
                .thenByDescending { it.isOnline }
                .thenBy { it.name.lowercase(Locale.ROOT) }
                .thenBy { it.id }
        )

        return AppResult.Success(sorted)
    }
}

fun interface DeviceProvider {
    fun getDevices(): AppResult<List<HubItem.Device>>
}

fun interface ChannelProvider {
    fun getChannels(): AppResult<List<HubItem.Channel>>
}
