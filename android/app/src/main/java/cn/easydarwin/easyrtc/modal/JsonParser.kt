package cn.easydarwin.easyrtc.modal

import kotlinx.serialization.json.Json

object JsonParser {
    private val json = Json {
        ignoreUnknownKeys = true // 忽略未知键
        isLenient = true // 宽松模式
        prettyPrint = true // 美化输出
    }

    fun parseResponse(jsonString: String): ApiResponse {
        return json.decodeFromString(jsonString)
    }
}