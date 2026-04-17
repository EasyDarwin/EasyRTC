package cn.easydarwin.easyrtc.domain.usecase

import cn.easydarwin.easyrtc.core.result.AppResult
import cn.easydarwin.easyrtc.domain.model.HubItem
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class GetHubItemsUseCaseTest {

    @Test
    fun execute_merges_and_sorts_consistently() {
        val useCase = GetHubItemsUseCase(
            deviceProvider = DeviceProvider {
                AppResult.Success(
                    listOf(
                        HubItem.Device(
                            id = "d2",
                            name = "Zeta Device",
                            isOnline = false,
                            ip = "10.0.0.2",
                            channelCount = 1,
                            model = "M2",
                            manufacturer = "Vendor"
                        ),
                        HubItem.Device(
                            id = "d1",
                            name = "Alpha Device",
                            isOnline = true,
                            ip = "10.0.0.1",
                            channelCount = 2,
                            model = "M1",
                            manufacturer = "Vendor"
                        )
                    )
                )
            },
            channelProvider = ChannelProvider {
                AppResult.Success(
                    listOf(
                        HubItem.Channel(
                            id = "c2",
                            name = "Zulu Channel",
                            isOnline = false,
                            transport = "UDP",
                            createdAt = "2026-01-01",
                            deviceId = "d2"
                        ),
                        HubItem.Channel(
                            id = "c1",
                            name = "Beta Channel",
                            isOnline = true,
                            transport = "TCP",
                            createdAt = "2026-01-02",
                            deviceId = "d1"
                        )
                    )
                )
            }
        )

        val result = useCase.execute()

        assertTrue(result is AppResult.Success)
        val items = (result as AppResult.Success).data
        assertEquals(4, items.size)
        assertEquals("d1", items[0].id)
        assertEquals("d2", items[1].id)
        assertEquals("c1", items[2].id)
        assertEquals("c2", items[3].id)
    }

    @Test
    fun execute_returns_device_error_immediately() {
        val useCase = GetHubItemsUseCase(
            deviceProvider = DeviceProvider { AppResult.Error("device failed") },
            channelProvider = ChannelProvider {
                AppResult.Success(
                    listOf(
                        HubItem.Channel(
                            id = "c1",
                            name = "Channel",
                            isOnline = true,
                            transport = "TCP",
                            createdAt = "2026-01-02",
                            deviceId = "d1"
                        )
                    )
                )
            }
        )

        val result = useCase.execute()
        assertTrue(result is AppResult.Error)
        assertEquals("device failed", (result as AppResult.Error).message)
    }

    @Test
    fun execute_returns_channel_error_after_devices_success() {
        val useCase = GetHubItemsUseCase(
            deviceProvider = DeviceProvider {
                AppResult.Success(
                    listOf(
                        HubItem.Device(
                            id = "d1",
                            name = "Device",
                            isOnline = true,
                            ip = "10.0.0.1",
                            channelCount = 1,
                            model = "M1",
                            manufacturer = "Vendor"
                        )
                    )
                )
            },
            channelProvider = ChannelProvider { AppResult.Error("channel failed") }
        )

        val result = useCase.execute()
        assertTrue(result is AppResult.Error)
        assertEquals("channel failed", (result as AppResult.Error).message)
    }

    @Test
    fun execute_sorts_names_case_insensitively_and_uses_id_tiebreaker() {
        val useCase = GetHubItemsUseCase(
            deviceProvider = DeviceProvider {
                AppResult.Success(
                    listOf(
                        HubItem.Device(
                            id = "d2",
                            name = "alpha",
                            isOnline = true,
                            ip = "10.0.0.2",
                            channelCount = 1,
                            model = "M2",
                            manufacturer = "Vendor"
                        ),
                        HubItem.Device(
                            id = "d1",
                            name = "ALPHA",
                            isOnline = true,
                            ip = "10.0.0.1",
                            channelCount = 1,
                            model = "M1",
                            manufacturer = "Vendor"
                        )
                    )
                )
            },
            channelProvider = ChannelProvider {
                AppResult.Success(
                    listOf(
                        HubItem.Channel(
                            id = "c2",
                            name = "beta",
                            isOnline = true,
                            transport = "TCP",
                            createdAt = "2026-01-02",
                            deviceId = "d1"
                        ),
                        HubItem.Channel(
                            id = "c1",
                            name = "BETA",
                            isOnline = true,
                            transport = "TCP",
                            createdAt = "2026-01-02",
                            deviceId = "d1"
                        )
                    )
                )
            }
        )

        val result = useCase.execute()

        assertTrue(result is AppResult.Success)
        val items = (result as AppResult.Success).data
        assertEquals(listOf("d1", "d2", "c1", "c2"), items.map { it.id })
    }

    @Test
    fun execute_does_not_call_channel_provider_when_device_provider_fails() {
        var isChannelProviderCalled = false
        val useCase = GetHubItemsUseCase(
            deviceProvider = DeviceProvider { AppResult.Error("device failed") },
            channelProvider = ChannelProvider {
                isChannelProviderCalled = true
                AppResult.Success(emptyList())
            }
        )

        val result = useCase.execute()

        assertTrue(result is AppResult.Error)
        assertFalse(isChannelProviderCalled)
    }
}
