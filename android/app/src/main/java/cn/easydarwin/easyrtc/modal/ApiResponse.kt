package cn.easydarwin.easyrtc.modal

import kotlinx.serialization.Serializable

// 主响应结构
@Serializable
data class ApiResponse(
//    val devices: List<User>
    val devices: List<User>? = emptyList()
)

@Serializable
data class User(val id: String, val name: String)

