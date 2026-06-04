package cn.easydarwin.easyrtc.utils

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.text.method.ScrollingMovementMethod
import android.widget.TextView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import okhttp3.OkHttpClient
import okhttp3.Request
import java.util.concurrent.TimeUnit

@Serializable
data class AppUpdateInfo(
    val version: String = "",
    val isPush: Boolean = false,
    val name: String = "",
    val type: String = "",
    val url: String = "",
    val items: List<String> = emptyList()
)

object AppUpdateChecker {

    private const val UPDATE_URL = "https://www.easyrtc.cn/public/an_version.json"
    private val json = Json { ignoreUnknownKeys = true }

    private val client = OkHttpClient.Builder()
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(10, TimeUnit.SECONDS)
        .build()

    suspend fun check(activity: Activity) {
        val info = fetchUpdateInfo() ?: return
        val localVersion = getLocalVersionName(activity) ?: return
        if (!isNewer(info.version, localVersion)) return

        withContext(Dispatchers.Main) {
            showUpdateDialog(activity, info)
        }
    }

    private suspend fun fetchUpdateInfo(): AppUpdateInfo? = withContext(Dispatchers.IO) {
        try {
            val request = Request.Builder().url(UPDATE_URL).build()
            val response = client.newCall(request).execute()
            if (!response.isSuccessful) return@withContext null
            var body = response.body?.string() ?: return@withContext null
//            body = "{\n" +
//                    "\t\"version\": \"1.999.1\",\n" +
//                    "\t\"isPush\": true,\n" +
//                    "\t\"name\": \"EasyRTC-Android\",\n" +
//                    "\t\"type\": \"Android\",\n" +
//                    "\t\"url\": \"https://xxrb.com:19892/easyrtc.1.0.1.26.0525.apk\",\n" +
//                    "\t\"items\": [\n" +
//                    "\t\t\"[修复] 支持最新Chrome浏览器\",\n" +
//                    "\t\t\"[新增] 支持IP直连\",\n" +
//                    "\t\t\"[新增] 支持WHIP推流\",\n" +
//                    "\t\t\"[优化] 双向通话体验\"\n" +
//                    "      ]\n" +
//                    "}\n"
            json.decodeFromString<AppUpdateInfo>(body)
        } catch (e: Exception) {
            AppLogStore.appendCritical("AppUpdateChecker", "fetch failed: ${e.message}")
            null
        }
    }

    private fun getLocalVersionName(activity: Activity): String? {
        return try {
            activity.packageManager.getPackageInfo(activity.packageName, 0)
                .versionName?.removePrefix("V")
        } catch (_: Exception) { null }
    }

    private fun isNewer(remote: String, local: String): Boolean {
        val r = remote.split(".").mapNotNull { it.toIntOrNull() }
        val l = local.split(".").mapNotNull { it.toIntOrNull() }
        for (i in 0 until maxOf(r.size, l.size)) {
            val rv = r.getOrElse(i) { 0 }
            val lv = l.getOrElse(i) { 0 }
            if (rv > lv) return true
            if (rv < lv) return false
        }
        return false
    }

    private fun showUpdateDialog(activity: Activity, info: AppUpdateInfo) {
        if (activity.isFinishing || activity.isDestroyed) return

        val changelog = info.items.joinToString("\n")
        val message = if (changelog.isNotBlank()) "发现新版本 v${info.version}\n\n$changelog" else "发现新版本 v${info.version}"

        val dialog = MaterialAlertDialogBuilder(activity)
            .setTitle("版本更新")
            .setMessage(message)
            .setPositiveButton("立即更新") { _, _ ->
                try {
                    activity.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(info.url)))
                } catch (_: Exception) { }
                activity.finish()
            }

        if (!info.isPush) {
            dialog.setNegativeButton("稍后再说", null)
        }

        dialog.setCancelable(!info.isPush)
        dialog.show().let { alertDialog ->
            val tv = alertDialog.findViewById<TextView>(android.R.id.message)
            tv?.movementMethod = ScrollingMovementMethod.getInstance()
            tv?.maxHeight = (activity.resources.displayMetrics.heightPixels * 0.4).toInt()
        }
    }
}
