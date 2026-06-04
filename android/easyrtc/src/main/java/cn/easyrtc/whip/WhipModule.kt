package cn.easyrtc.whip

import android.util.Log
import cn.easyrtc.EasyRTCLog
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import kotlin.concurrent.thread

class WhipModule(private val whipUrl: String) {

    companion object {
        private const val TAG = "WhipModule"
    }

    private  var isCanceled = false

    private val client = OkHttpClient()
    fun postOffer(
        offerSdp: String,
        onSuccess: (answerSdp: String) -> Unit,
        onError: (String) -> Unit,
    ) {
        EasyRTCLog.i(TAG, "postOffer: url=$whipUrl offerSdpLength=${offerSdp.length}")
        thread {
            try {
                val requestBody = offerSdp.toRequestBody()
                val request = Request.Builder()
                    .url(whipUrl)
                    .post(requestBody)
                    .addHeader("Content-Type", "application/sdp")
                    .build()

                val response = client.newCall(request).execute()

                EasyRTCLog.i(TAG, "postOffer response: code=${response.code} message=${response.message}, canceled:${isCanceled}")
                if (!isCanceled){
                    if (response.isSuccessful) {
                        val answerSdp = response.body?.string().orEmpty()
                        if (answerSdp.isBlank()) {
                            EasyRTCLog.w(TAG, "postOffer: empty answer sdp")
                            onError("WHIP 响应为空")
                        } else {
                            EasyRTCLog.i(TAG, "postOffer success: answerSdpLength=${answerSdp.length}")
                            onSuccess(answerSdp)
                        }
                    } else {
                        EasyRTCLog.e(TAG, "postOffer failed: code=${response.code} bodyMessage=${response.message}")
                        onError("服务器返回错误: ${response.code} ${response.message}")
                    }
                }
                response.close()
            } catch (e: Exception) {
                Log.e(TAG, "发送 Offer 失败", e)
                EasyRTCLog.e(TAG, "postOffer exception", e)
                onError("发送 Offer 失败: ${e.message}")
            }
        }
    }

    fun cancel(){
        isCanceled = true
    }
}
