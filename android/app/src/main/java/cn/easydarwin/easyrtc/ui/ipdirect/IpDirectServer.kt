package cn.easydarwin.easyrtc.ui.ipdirect

import android.os.Handler
import android.os.Looper
import android.util.Log
import org.java_websocket.WebSocket
import org.java_websocket.handshake.ClientHandshake
import org.java_websocket.server.WebSocketServer
import java.net.InetSocketAddress
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * WebSocket server for IP直连 mode.
 *
 * Listens on [port] (default 19005) and accepts a single active client.
 * Binary signaling protocol (compatible with easyrtc.cn JS client):
 *   Client → Server: 36-byte handshake (msgtype=0x00010005)
 *   Server → Client: binary offer (SDP embedded, client finds "v=0")
 *   Client → Server: NTI_WEBRTCANSWER_INFO binary answer
 */
class IpDirectServer(
    private val port: Int = DEFAULT_PORT,
    private val listener: Listener
) {

    companion object {
        private const val TAG = "IpDirectServer"
        const val DEFAULT_PORT = 19005

        // Message types in binary protocol
        private const val MSG_HANDSHAKE = 0x00010005
        private const val MSG_ANSWER = 0x10008
    }

    interface Listener {
        fun onClientConnected(remoteAddress: String)
        fun onClientDisconnected(remoteAddress: String)
        /** Client sent handshake — server should create and send offer */
        fun onClientReady()
        /** Client sent answer SDP */
        fun onAnswerReceived(sdp: String)
        fun onServerStarted(port: Int)
        fun onServerStopped()
        fun onServerError(message: String)
    }

    private var server: InnerServer? = null
    private var activeClient: WebSocket? = null
    private val mainHandler = Handler(Looper.getMainLooper())

    val isRunning: Boolean get() = server != null

    fun start() {
        if (server != null) {
            Log.w(TAG, "Server already running")
            return
        }
        try {
            val ws = InnerServer(InetSocketAddress(port))
            ws.isReuseAddr = true
            ws.start()
            server = ws
            Log.d(TAG, "Server starting on port $port")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start server: ${e.message}")
            postOnMain { listener.onServerError("启动失败: ${e.message}") }
        }
    }

    fun stop() {
        activeClient = null
        server?.let { s ->
            try {
                s.stop(500)
            } catch (e: Exception) {
                Log.e(TAG, "Error stopping server: ${e.message}")
            }
        }
        server = null
        postOnMain { listener.onServerStopped() }
    }

    /**
     * Send offer SDP to client as raw binary (no wrapper).
     * Client uses getPureSdpFromBuffer() which scans for "v=0"
     * and extracts to end-of-buffer — sending raw SDP works directly.
     */
    fun sendOffer(sdp: String) {
        val client = activeClient
        if (client == null || !client.isOpen) {
            Log.w(TAG, "No active client to send offer")
            return
        }
        val sdpBytes = sdp.toByteArray(Charsets.UTF_8)
        client.send(sdpBytes)
        Log.d(TAG, "Offer sent, sdplen=${sdpBytes.size}")
    }

    fun sendHangup() {
        val client = activeClient
        if (client == null || !client.isOpen) return
        val buf = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN)
        buf.putInt(8)
        buf.putInt(0x10009)
        client.send(buf.array())
        client.close()
        activeClient = null
        Log.d(TAG, "Hangup sent, client closed")
    }

    private fun postOnMain(action: () -> Unit) {
        mainHandler.post(action)
    }

    private fun parseBinaryMessage(data: ByteArray) {
        if (data.size < 8) return
        val buf = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN)
        val msgtype = buf.getInt(4)

        when (msgtype) {
            MSG_HANDSHAKE -> {
                Log.d(TAG, "Received handshake, size=${data.size}")
                postOnMain { listener.onClientReady() }
            }
            MSG_ANSWER -> {
                // NTI_WEBRTCANSWER_INFO format:
                // [totalLen:4][msgtype:4][hisid:16][sdplen:2][SDP][0x0a00]
                if (data.size < 28) {
                    Log.w(TAG, "Answer too short: ${data.size}")
                    return
                }
                val sdplen = buf.getShort(24).toInt() and 0xFFFF
                if (sdplen <= 0 || 26 + sdplen > data.size) {
                    Log.w(TAG, "Invalid sdplen=$sdplen, data.size=${data.size}")
                    return
                }
                val sdp = String(data, 26, sdplen, Charsets.UTF_8)
                Log.d(TAG, "Received answer, sdplen=$sdplen")
                postOnMain { listener.onAnswerReceived(sdp) }
            }
            else -> {
                Log.w(TAG, "Unknown msgtype: 0x${msgtype.toString(16)}")
            }
        }
    }

    private inner class InnerServer(address: InetSocketAddress) : WebSocketServer(address) {

        override fun onStart() {
            Log.d(TAG, "WebSocket server started on port $port")
            postOnMain { listener.onServerStarted(port) }
        }

        override fun onOpen(conn: WebSocket, handshake: ClientHandshake) {
            val addr = conn.remoteSocketAddress?.toString() ?: "unknown"
            Log.d(TAG, "Client connected: $addr")

            if (activeClient != null && activeClient!!.isOpen) {
                Log.w(TAG, "Rejecting second client, already have active session")
                conn.close(4001, "Server busy")
                return
            }

            activeClient = conn
            postOnMain { listener.onClientConnected(addr) }
        }

        override fun onClose(conn: WebSocket, code: Int, reason: String, remote: Boolean) {
            val addr = conn.remoteSocketAddress?.toString() ?: "unknown"
            Log.d(TAG, "Client disconnected: $addr code=$code reason=$reason")
            if (conn == activeClient) {
                activeClient = null
                postOnMain { listener.onClientDisconnected(addr) }
            }
        }

        override fun onMessage(conn: WebSocket, message: ByteBuffer) {
            val bytes = ByteArray(message.remaining())
            message.get(bytes)
            Log.d(TAG, "Binary message: ${bytes.size} bytes")
            parseBinaryMessage(bytes)
        }

        override fun onMessage(conn: WebSocket, message: String) {
            // Legacy text messages: treat "PING" as keepalive
            if ("PING" == message) return
            Log.d(TAG, "Text message: ${message.take(200)}")
            if (message.startsWith("{")) {
                // Fallback JSON parsing for backward compatibility
                try {
                    val json = org.json.JSONObject(message)
                    when (json.optString("type")) {
                        "offer" -> postOnMain { listener.onAnswerReceived(json.getString("sdp")) }
                        else -> Log.w(TAG, "Unknown JSON type: ${json.optString("type")}")
                    }
                } catch (e: Exception) {
                    Log.e(TAG, "Failed to parse text message: ${e.message}")
                }
            }
        }

        override fun onError(conn: WebSocket?, ex: Exception) {
            Log.e(TAG, "Server error: ${ex.message}")
            postOnMain { listener.onServerError(ex.message ?: "unknown error") }
        }
    }
}
