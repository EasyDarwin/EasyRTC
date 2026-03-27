package cn.easydarwin.easyrtc.utils
import android.util.Log

object TimeCost {
    /**
     * 统计代码块的耗时（毫秒），返回执行结果与耗时。
     */
    inline fun <T> measureMillis(tag: String = "TimeCost", log: Boolean = true, block: () -> T): Pair<T, Long> {
        val start = System.currentTimeMillis()
        val result = block()
        val cost = System.currentTimeMillis() - start
        if (log) Log.d(tag, "耗时: ${cost}ms")
        return result to cost
    }

    /**
     * 统计代码块的耗时（纳秒），适用于更高精度的需求。
     */
    inline fun <T> measureNanos(tag: String = "TimeCost", log: Boolean = true, block: () -> T): Pair<T, Long> {
        val start = System.nanoTime()
        val result = block()
        val cost = System.nanoTime() - start
        if (log) Log.d(tag, "耗时: ${cost}ns")
        return result to cost
    }
}
