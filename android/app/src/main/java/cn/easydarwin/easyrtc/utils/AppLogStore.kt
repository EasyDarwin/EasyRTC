package cn.easydarwin.easyrtc.utils

import android.content.Context
import android.util.Log
import java.io.File
import java.io.IOException
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object AppLogStore {

    private const val TAG = "AppLogStore"
    private const val MESSAGE_FILE_NAME = "message.txt"
    private const val MAX_LOG_LENGTH = 100 * 1024
    private val lock = Any()
    private val builder = StringBuilder()
    @Volatile
    private var appContext: Context? = null

    fun init(context: Context) {
        appContext = context.applicationContext
    }

    fun appendTimestamped(message: String) {
        val formatter = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())
        append("${formatter.format(Date())}: $message\n")
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
            persist(value)
        }
    }

    private fun persist(value: String) {
        val file = resolveMessageFile() ?: return
        try {
            file.parentFile?.mkdirs()
            file.appendText(value, Charsets.UTF_8)
        } catch (e: IOException) {
            Log.e(TAG, "Failed to persist message", e)
        }
    }

    private fun resolveMessageFile(): File? {
        val context = appContext ?: return null
        val rootDir = context.getExternalFilesDir(null) ?: context.filesDir
        return File(rootDir, MESSAGE_FILE_NAME)
    }
}
