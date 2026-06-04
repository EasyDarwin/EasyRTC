package cn.easyrtc

import android.util.Log

object EasyRTCLog {
    enum class Level { DEBUG, INFO, WARN, ERROR }

    @Volatile
    private var sink: ((Level, String, String, Throwable?) -> Unit)? = null

    fun setSink(logger: ((Level, String, String, Throwable?) -> Unit)?) {
        sink = logger
    }

    fun d(tag: String, message: String) = emit(Level.DEBUG, tag, message, null)
    fun i(tag: String, message: String) = emit(Level.INFO, tag, message, null)
    fun w(tag: String, message: String) = emit(Level.WARN, tag, message, null)
    fun e(tag: String, message: String, throwable: Throwable? = null) =
        emit(Level.ERROR, tag, message, throwable)

    private fun emit(level: Level, tag: String, message: String, throwable: Throwable?) {
        when (level) {
            Level.DEBUG -> Log.d(tag, message)
            Level.INFO -> Log.i(tag, message)
            Level.WARN -> Log.w(tag, message)
            Level.ERROR -> Log.e(tag, message, throwable)
        }
        sink?.invoke(level, tag, message, throwable)
    }
}
