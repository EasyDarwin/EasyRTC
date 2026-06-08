package cn.easydarwin.easyrtc.utils

import android.content.Context
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import cn.easyrtc.NativeLogBridge
import android.util.Log

object AppLogStore {

    private const val TAG = "AppLogStore"
    private const val MAX_LOG_LENGTH = 100 * 1024

    fun init(@Suppress("UNUSED_PARAMETER") context: Context) {}

    fun appendTimestamped(message: String) {
        val formatter = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())
        append(message)
    }

    fun appendCritical(tag: String, message: String, throwable: Throwable? = null) {
        val thread = Thread.currentThread().name
        val payload = buildString {
            append("[CRITICAL][$tag][thread=$thread] $message")
            if (throwable != null) {
                append(" | exception=${throwable.javaClass.simpleName}: ${throwable.message}")
            }
        }
        appendTimestamped(payload)
    }

    fun appendRaw(message: String) {
        append(message)
    }


    private fun append(value: String) {
        NativeLogBridge.dispatch(value)
    }
}
