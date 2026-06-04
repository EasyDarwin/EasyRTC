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
    private val lock = Any()
    private val builder = StringBuilder()
    
    fun init(@Suppress("UNUSED_PARAMETER") context: Context) {}

    fun appendTimestamped(message: String) {
        val formatter = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())
        append("${formatter.format(Date())}: $message\n")
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

    fun text(): String {
        synchronized(lock) {
            return builder.toString()
        }
    }

    fun isEmpty(): Boolean {
        synchronized(lock) {
            return builder.isEmpty()
        }
    }

    private fun append(value: String) {
        synchronized(lock) {
            builder.append(value)
            if (builder.length > MAX_LOG_LENGTH) {
                builder.delete(0, builder.length - MAX_LOG_LENGTH)
            }
            NativeLogBridge.dispatch(value)
        }
    }
}
