package cn.easydarwin.easyrtc.utils

import android.os.Handler
import android.os.Looper
import android.util.Log
import cn.easyrtc.EasyRTCCodec
import cn.easyrtc.EasyRTCDirection
import cn.easyrtc.EasyRTCIceTransportPolicy
import cn.easyrtc.EasyRTCSdk
import cn.easyrtc.EasyRTCStreamTrack
import cn.easyrtc.EasyRTCUser
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import okio.ByteString
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.util.concurrent.TimeUnit
import kotlin.let

public class WebSocketManager(private val url: String, private val token: String? = null, private val callback: WebSocketCallback) {

    private var webSocket: WebSocket? = null

    private val client = OkHttpClient.Builder().pingInterval(0, TimeUnit.SECONDS).readTimeout(0, TimeUnit.SECONDS).build()

    var uuidClientA = ""
    var uuidClientB = ""

    private val handler = Handler(Looper.getMainLooper())
    private var onlineTask: Runnable? = null
    private var isOnlineTaskRunning = false


    private var reconnectTask: Runnable? = null
    private var isReconnecting = false
    private var allowReconnect = true
    private var reconnectDelay = 3000L // 初始重连延迟 3 秒
    private val maxReconnectDelay = 60000L // 最大重连延迟 60 秒


    fun connect() {
        val builder = Request.Builder().url(url)
        token?.let {
            builder.addHeader("Authorization", "Bearer $it")
        }
        webSocket = client.newWebSocket(builder.build(), listener)
    }


    fun sendBinary(data: ByteArray) {
//        bayLog(data, "发送数据")
        webSocket?.send(ByteString.of(*data))
    }

    fun close() {
        webSocket?.close(1000, "normal close")
    }

    fun shutdown() {
        allowReconnect = false
        stopReconnectTask()
        stopOnlineClientsTask()
        webSocket?.cancel()
        webSocket?.close(1000, "shutdown")
        webSocket = null
    }

    interface WebSocketCallback {
        fun onWSConnected()
        fun onWSMessage(text: String)
        fun onWSDisconnected(code: Int, reason: String)
        fun onWSError(error: Throwable)
        fun onWSOnlineUsers(users: List<EasyRTCUser>)
        fun onWSLogs(txt: String)
        fun onWSIncomingCall(uuid: String)
        fun onTypeAndData(type: Int, data: ByteArray)
    }

    private val listener = object : WebSocketListener() {

        override fun onOpen(ws: WebSocket, response: Response) {
            reconnectDelay = 3000L
            isReconnecting = false
            stopReconnectTask()
            //TODO 自行注册
            register(SPUtil.getInstance().rtcUserUUID)
//            Log.d(TAG, "注册成功 ${SPUtil.getInstance().rtcUserUUID}")
            startOnlineClientsTask()
            callback.onWSConnected()
        }

        override fun onMessage(ws: WebSocket, text: String) {
            callback.onWSMessage(text)
        }

        override fun onMessage(ws: WebSocket, bytes: ByteString) {
//            val data = bytes.toByteArray()
//            val hexString = data.joinToString(" ") { "%02X".format(it.toInt() and 0xFF) }
//            Log.d(TAG, "收到字节数据: $hexString")
            handlerEventByteArray(bytes.toByteArray())
        }

        override fun onClosing(ws: WebSocket, code: Int, reason: String) {
            ws.close(code, reason)
            stopOnlineClientsTask()
            callback.onWSDisconnected(code, reason)

            attemptReconnect()
        }

        override fun onFailure(ws: WebSocket, t: Throwable, response: Response?) {
            stopOnlineClientsTask()
            callback.onWSError(t)
            attemptReconnect()
//            Log.e("HomeFragment", "onFailure: ${t.message}")
        }
    }

    fun register(uuid: String = "92092eea-be8d-4ec4-8ac5-123456789001") {
        this.uuidClientA = uuid
        val data = handlerRegister(uuid);
        sendBinary(data)
    }

    fun call(uuid: String) {
        4 + 4 + 16 + 32 + 2 + 2 + 0
//        Log.d(TAG, "uuid = $uuid")
        val length = fillBytes(60, 4)
        val msgType = fillBytes(65539, 4)
        val hisid = uuidToByteArray(uuid)
        val hiskey = ByteArray(32)
        val extradatalen0 = ByteArray(2)
        val extradatalen1 = ByteArray(2)
        val extradata0 = ByteArray(0)

        val data = ByteArrayOutputStream()
        data.write(length)
        data.write(msgType)
        data.write(hisid)
        data.write(hiskey)
        data.write(extradatalen0)
        data.write(extradatalen1)
        data.write(extradata0)
        sendBinary(data.toByteArray())
    }

    fun bayLog(data: ByteArray, tag: String = "输出") {
        Log.d(TAG, "$tag = ${data.joinToString(" ") { "%02X".format(it) }}")
    }

    fun getOnlineClients() {
        val msgType = fillBytes(131073, 4)
        val length = fillBytes(8, 4)
        val bos = ByteArrayOutputStream()
        bos.write(length)
        bos.write(msgType)
        sendBinary(bos.toByteArray())
    }

    fun handlerPeerConnection(data: ByteArray, isCaller: Boolean = false): String {
//        Log.d(TAG, " handlerPeerConnection ...")
        val uuid = byteArrayToUuid(data.copyOfRange(8, 24))
        val exTraDataLen = bytesToIntLE(data.copyOfRange(24, 26))
        val strDataLen = bytesToIntLE(data.copyOfRange(26, 28))
        val strCount = bytesToIntLE(data.copyOfRange(28, 29))
        val sturnTypes = data.copyOfRange(29, 37)
        val sturnDatas = splitByZero(data.copyOfRange(37, 37 + strDataLen))  // sturndatas 数据
        val mStunTurnInfo = StunTurnInfo()
        for ((index, b) in sturnTypes.withIndex()) {
            if (index >= 4) break;
            when (b.toInt() and 0xFF) {
                0x00 -> mStunTurnInfo.stunServer = sturnDatas.getOrNull(index) ?: ""
                0x01, 0x04 -> mStunTurnInfo.turnServer = sturnDatas.getOrNull(index) ?: ""
                0x02 -> mStunTurnInfo.turnUsername = sturnDatas.getOrNull(index) ?: ""
                0x03 -> mStunTurnInfo.turnPassword = sturnDatas.getOrNull(index) ?: ""
            }
        }
//        Log.d(TAG, " mStunTurnInfo= ${mStunTurnInfo.toString()},uuid = $uuid")
        this.uuidClientB = uuid
        EasyRTCSdk.setEncoderConfig(
            cn.easyrtc.model.VideoEncodeConfig(
                SPUtil.getInstance().getIsHevc(),
                SPUtil.getInstance().getVideoFrameRate(),
                SPUtil.getInstance().cameraId,
                SPUtil.getInstance().getVideoResolution(),
                cn.easyrtc.model.VideoEncodeConfig.ORIENTATION_90,
                SPUtil.getInstance().getVideoBitRateKbps()
            )
        )
        EasyRTCSdk.connection(
            "stun:${mStunTurnInfo.stunServer}",
            "turn:${mStunTurnInfo.turnServer}",
            mStunTurnInfo.turnUsername,
            mStunTurnInfo.turnPassword,
            1,
            EasyRTCIceTransportPolicy.EaSyRTC_ICE_TRANSPORT_POLICY_ALL,
            0L
        );
        if (isCaller) return String(data.copyOfRange(37 + strDataLen, data.size), Charsets.UTF_8)
        return ""
    }


    fun handlerCallerSDP(sdp: String) {
        if (sdp.isNotEmpty()) {
            var videoCodec = 0
            var audioCodec = 0

            if (sdp.contains("m=video", ignoreCase = true)) {
//                Log.d(TAG, "handlerSDP sdp=${SPUtil.Companion.getInstance().hevaddDataChannelcCodec}")
                var codeID = 0;
                if (sdp.contains("H264/90000")) {
                    codeID = EasyRTCCodec.H264
//                    remoteRTCHelper?.reinitVideoDecoder(RemoteRTCHelper.CODEC_H264)
                } else if (sdp.contains("H265/90000")) {
//                    remoteRTCHelper?.reinitVideoDecoder(RemoteRTCHelper.CODEC_H265)
                    codeID = EasyRTCCodec.H265
                } else if (sdp.contains("VP8/90000")) {
                    codeID = EasyRTCCodec.VP8
                }
                if (codeID != 0) videoCodec = codeID
            }

            if (sdp.contains("m=audio", ignoreCase = true)) {
                var codeID = 0;
                if (sdp.contains("PCMA/8000", ignoreCase = true)) {
                    codeID = EasyRTCCodec.ALAW
                } else if (sdp.contains("PCMU/8000", ignoreCase = true)) {
                    codeID = EasyRTCCodec.MULAW
                } else if (sdp.contains("opus/48000", ignoreCase = true)) {
                    codeID = EasyRTCCodec.OPUS
                }
                if (codeID != 0) audioCodec = codeID

            }

            if (videoCodec == 0) videoCodec = if (SPUtil.getInstance().getIsHevc()) EasyRTCCodec.H265 else EasyRTCCodec.H264
            if (audioCodec == 0) audioCodec = EasyRTCCodec.ALAW

            val session = EasyRTCSdk.getMediaSession()
            //      try to clean old first
            session.removeTransceivers()
            session.addTransceivers(videoCodec, audioCodec)

            if (sdp.contains("webrtc-datachannel", ignoreCase = true)) EasyRTCSdk.addDataChannel()
            EasyRTCSdk.createAnswer(sdp)  //创建 Answer 的 SDP
        }

    }

    fun handlerEventByteArray(data: ByteArray) {
        val length = bytesToIntLE(data.copyOfRange(0, 4))
        val type = bytesToIntLE(data.copyOfRange(4, 8))
//        Log.d(TAG, "数据长度 : length =  $length type = $type")
        //登录返回解析
        if (HPACKLOGINUSERINFO == type) {
            val uuid = byteArrayToUuid(data.copyOfRange(8, 24))
            val status = bytesToIntLE(data.copyOfRange(24, length))
            val json = JSONObject()
            // 可以自行设定，比如为0登录成功,-1:uuid账号不存在; -2:uuid携带的mysn不正确，等等
            json.put("code", HPACKLOGINUSERINFO)
            json.put("msg", "$uuid 登录成功")
            json.put("status", status)
            callback.onWSMessage(json.toString())
            // 建立 peer  stun 消息解析
        } else if (ACKCONNECTUSERINF0 == type) {
            this.uuidClientB = byteArrayToUuid(data.copyOfRange(8, 24))
            this.uuidClientA = byteArrayToUuid(data.copyOfRange(24, 40))
            val status = bytesToIntLE(data.copyOfRange(40, 44))
            val extradatalen = bytesToIntLE(data.copyOfRange(44, 46))
            val extradata = String(data.copyOfRange(46, 46 + extradatalen), Charsets.UTF_8)
            Log.d(TAG, "播放 ack uuid=${this.uuidClientB} , play_uuid= ${this.uuidClientA} status=$status extradatalen=$extradatalen extradata=$extradata")
        } else if (HPNTIWEBRTCANSWERINFO == type) {
            this.uuidClientB = byteArrayToUuid(data.copyOfRange(8, 24))   //16字节
            val sdplen = bytesToIntLE(data.copyOfRange(24, 26))           //2字节
            val answerSdp = String(data.copyOfRange(26, 26 + sdplen), Charsets.UTF_8)
            EasyRTCSdk.setRemoteDescription(answerSdp)
        } else if (HPACKGETONLINEDEVICESINFO == type) {
            val users = mutableListOf<EasyRTCUser>()
            if (data.size < 12) return
            val count = bytesToIntLE(data.copyOfRange(8, 12))
            var offset = 12
            for (i in 0 until count) {
                // 每个用户 16 字节 UUID
                if (offset + 16 > data.size) break
                val uuidBytes = data.copyOfRange(offset, offset + 16)
                val uuid = byteArrayToUuid(uuidBytes)
                users.add(EasyRTCUser(uuid = uuid, username = uuid))
                offset += 16
            }
            callback.onWSOnlineUsers(users)
        }else {
            callback.onTypeAndData(type, data)
        }
    }

    fun sendOfferSDP(sdp: String, isOffer: Boolean) {
//        Log.d(TAG, "sdp = : $sdp this.uuidClientB = ${this.uuidClientB}")
        val sdpBytes = sdp.toByteArray(Charsets.UTF_8)
        val sdpBytesLength = sdpBytes.size.toLong()
        val msgType = fillBytes(if (isOffer) 65542 else 65544, 4)
        val hisid = uuidToByteArray(this.uuidClientB)
        val sdplen = fillBytes(sdpBytesLength, 2)
        val length = fillBytes(26 + sdpBytesLength + 1, 4)
//        Log.d(TAG, "uuid=${this.uuidClientB}")

        val bos = ByteArrayOutputStream()
        bos.write(length)                   //4字节总的长度
        bos.write(msgType)                  //4类型  65542
        bos.write(hisid)                    // 16个字节  uuid
        bos.write(sdplen)                   //2字节 sdp length
        bos.write(sdpBytes)                 //sdp 数据
        bos.write(byteArrayOf(0x00))        //结束符
        sendBinary(bos.toByteArray())
    }

    fun handlerRegister(uuid: String): ByteArray {
        val length = fillBytes(76, 4)
        val msgType = fillBytes(65537, 4)
        val myid = uuidToByteArray(uuid)

        val mysn = fillBytes(0, 16)
        val mykey = fillBytes(0, 32)
        val extradatalen0 = fillBytes(0, 2)
        val extradatalen1 = fillBytes(0, 2)

        val bos = ByteArrayOutputStream()
        bos.write(length)
        bos.write(msgType)
        bos.write(myid)
        bos.write(mysn)
        bos.write(mykey)
        bos.write(extradatalen0)
        bos.write(extradatalen1)

        return bos.toByteArray()
    }

    fun startOnlineClientsTask() {
        if (isOnlineTaskRunning) return
        isOnlineTaskRunning = true
        onlineTask = object : Runnable {
            override fun run() {
                if (!isOnlineTaskRunning) return
                getOnlineClients()   // 👈 每次执行
                handler.postDelayed(this, 15 * 1000) // 5 秒
//                handler.postDelayed(this, 3600 * 1000) // 5 秒
            }
        }
        handler.post(onlineTask!!)
    }

    fun stopOnlineClientsTask() {
        isOnlineTaskRunning = false
        onlineTask?.let {
            handler.removeCallbacks(it)
        }
        onlineTask = null
    }

    /**
     * 将整数值填充到指定字节长度，默认小端（低位在前）
     *
     * @param value 待转换的整数值
     * @param targetSize 目标字节长度（如 4 表示 uint32_t）
     * @param bigEndian 是否大端，默认 false（即小端）
     * @return 补齐后的 ByteArray
     */
    fun fillBytes(value: Long, targetSize: Int = 4, bigEndian: Boolean = false): ByteArray {
        require(targetSize > 0) { "targetSize must be > 0" }

        val bytes = ByteArray(targetSize)
        if (bigEndian) {
            for (i in 0 until targetSize) {
                bytes[targetSize - 1 - i] = ((value shr (8 * i)) and 0xFF).toByte()
            }
        } else {
            for (i in 0 until targetSize) {
                bytes[i] = ((value shr (8 * i)) and 0xFF).toByte()
            }
        }
        return bytes
    }

    fun uuidToByteArray(uuid: String): ByteArray {
//        Log.d(TAG, "uuidToByteArray uuid =$uuid")
        val hex = uuid.replace("-", "")
        require(hex.length == 32)

        val bytes = ByteArray(16)
        for (i in 0 until 4) {
            // 每 4 字节一个 DWORD
            val part = hex.substring(i * 8, (i + 1) * 8)
            // 转换 4 字节并小端存储
            for (j in 0 until 4) {
                bytes[i * 4 + j] = part.substring((3 - j) * 2, (3 - j) * 2 + 2).toInt(16).toByte()
            }
        }
        return bytes
    }

    fun bytesToIntLE(bytes: ByteArray, show: Boolean = false): Int {
        require(bytes.size <= 4) { "最多支持4字节" }
        val hex = bytes.joinToString(" ") { String.format("%02X", it) }
        if (show) Log.d("WebSocketManager", "收到字节 $hex")
        var result = 0
        for (i in bytes.indices) {
            result = result or ((bytes[i].toInt() and 0xFF) shl (8 * i))
        }
        return result
    }

    fun byteArrayToUuid(bytes: ByteArray): String {
        require(bytes.size == 16) { "ByteArray 长度必须是 16" }

        val sb = StringBuilder()

        for (i in 0 until 4) {
            // 取出每个 DWORD 小端字节
            val offset = i * 4
            for (j in 3 downTo 0) { // 小端还原
                sb.append("%02x".format(bytes[offset + j].toInt() and 0xFF))
            }
        }

        val hex = sb.toString()
        // 格式化成标准 UUID
        return "${hex.substring(0, 8)}-${hex.substring(8, 12)}-${hex.substring(12, 16)}-" + "${hex.substring(16, 20)}-${hex.substring(20, 32)}"
    }

    fun splitByZero(data: ByteArray): List<String> {
        val result = mutableListOf<String>()
        var start = 0

        for (i in data.indices) {
            if (data[i] == 0.toByte()) {
                if (i > start) {
                    result.add(
                        String(data.copyOfRange(start, i), Charsets.UTF_8)
                    )
                }
                start = i + 1
            }
        }

        return result
    }

    // ==================== 重连逻辑 ====================
    private fun attemptReconnect() {
        if (!allowReconnect) return
        if (isReconnecting) return
        isReconnecting = true
        reconnectTask?.let { handler.removeCallbacks(it) }

        reconnectTask = object : Runnable {
            override fun run() {
                callback.onWSLogs("网络异常 尝试重连 WebSocket")
                connect()
                // 下次重连延迟指数增长，最多 maxReconnectDelay
                reconnectDelay = (reconnectDelay * 2).coerceAtMost(maxReconnectDelay)
                handler.postDelayed(this, reconnectDelay)
            }
        }
        handler.postDelayed(reconnectTask!!, reconnectDelay)
    }

    private fun stopReconnectTask() {
        reconnectTask?.let { handler.removeCallbacks(it) }
        reconnectTask = null
        isReconnecting = false
    }


    data class StunTurnInfo(
        var stunServer: String = "",      // STUN 地址，例如: 106.85.205.34:3478
        var turnServer: String = "",      // TURN 地址，例如: 106.85.205.34:3478?transport=udp
        var turnUsername: String = "",    // TURN 用户名
        var turnPassword: String = ""     // TURN 密码
    ) {
        override fun toString(): String {
            return "STUN: $stunServer\nTURN: $turnServer\nUsername: $turnUsername\nPassword: $turnPassword"
        }
    }

    companion object {
        private val TAG = "WebSocketManager"

        const val HPREQLOGINUSERINFO = 65537            //0x10001  登录
        const val HPACKLOGINUSERINFO = 65538            //0x10002  登录回复

        const val HPREQCONNECTUSERINFO = 65539          //0x10003  发送请求播放
        const val ACKCONNECTUSERINF0 = 65540            //0x10004  平台回复播放ACK 请求

        const val HPREQGETWEBRTCOFFERINFO = 65541       //0x10005  响应请求
        const val HPNTIWEBRTCOFFER = 65542              //0x10006  发送 sdp
        const val HPNTIWEBRTCOFFERINFO2 = 65543         //0x10007
        const val HPNTIWEBRTCANSWERINFO = 65544         //0x10008  收到对方 sdp

        const val HPREQGETONLINEDEVICESINFO = 131073      //0x20001  //获取设备列表请求
        const val HPACKGETONLINEDEVICESINFO = 131074      //0x20002  //回复设备列表
    }
}
