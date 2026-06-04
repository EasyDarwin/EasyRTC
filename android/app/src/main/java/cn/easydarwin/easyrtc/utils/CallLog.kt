package cn.easydarwin.easyrtc.utils

import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Call-level log store — stores individual log entries for list display.
 * Only captures call-related events (SDP, connection state, data channel messages, etc.).
 */
object CallLog {

    private const val MAX_ENTRIES = 500
    private val lock = Any()
    private val entries = ArrayList<String>(MAX_ENTRIES)

    @JvmStatic
    fun append(message: String) {
        val formatter = SimpleDateFormat("HH:mm:ss", Locale.getDefault())
        val line = "${formatter.format(Date())}  $message"
        synchronized(lock) {
            entries.add(line)
            if (entries.size > MAX_ENTRIES) {
                entries.removeAt(0)
            }
        }
    }

    @JvmStatic
    fun text(): String = synchronized(lock) { entries.joinToString("\n") }

    @JvmStatic
    fun entries(): List<String> = synchronized(lock) { ArrayList(entries) }
}
