package cn.easydarwin.easyrtc.ui.ipdirect

import android.os.Handler
import android.os.Looper
import android.util.Log
import org.java_websocket.WebSocket
import org.java_websocket.handshake.ClientHandshake
import org.java_websocket.server.WebSocketServer
import org.json.JSONObject
import java.net.InetSocketAddress

/**
 * WebSocket server for IP直连 mode.
 *
 * Listens on [port] (default 19005) and accepts a single active client.
 * Signaling uses a simple JSON protocol:
 *   Client → Server: {"type":"offer",  "sdp":"..."}
 *   Server → Client: {"type":"answer", "sdp":"..."}
 *   Either  → Either: {"type":"hangup"}
 */
class IpDirectServer(
    private val port: Int = DEFAULT_PORT,
    private val listener: Listener
) {

    companion object {
        private const val TAG = "IpDirectServer"
        const val DEFAULT_PORT = 19005
    }

    interface Listener {
        fun onClientConnected(remoteAddress: String)
        fun onClientDisconnected(remoteAddress: String)
        fun onOfferReceived(sdp: String)
        fun onHangup()
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

    fun sendAnswer(sdp: String) {
        val client = activeClient
        if (client == null || !client.isOpen) {
            Log.w(TAG, "No active client to send answer")
            return
        }
        val msg = JSONObject().apply {
            put("type", "answer")
            put("sdp", sdp)
        }
        client.send(msg.toString())
        Log.d(TAG, "Answer SDP sent, length=${sdp.length}")
    }

    fun sendHangup() {
        val client = activeClient
        if (client == null || !client.isOpen) return
        val msg = JSONObject().apply { put("type", "hangup") }
        client.send(msg.toString())
    }

    private fun postOnMain(action: () -> Unit) {
        mainHandler.post(action)
    }

    private inner class InnerServer(address: InetSocketAddress) : WebSocketServer(address) {

        override fun onStart() {
            Log.d(TAG, "WebSocket server started on port $port")
            postOnMain { listener.onServerStarted(port) }
        }

        override fun onOpen(conn: WebSocket, handshake: ClientHandshake) {
            val addr = conn.remoteSocketAddress?.toString() ?: "unknown"
            Log.d(TAG, "Client connected: $addr")

            // Only allow one active client
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

        override fun onMessage(conn: WebSocket, message: String) {
            Log.d(TAG, "Message from client: ${message.take(200)}")
            try {
                val json = JSONObject(message)
                when (json.optString("type")) {
                    "offer" -> {
                        val sdp = json.getString("sdp")
                        postOnMain { listener.onOfferReceived(sdp) }
                    }
                    "hangup" -> {
                        postOnMain { listener.onHangup() }
                    }
                    else -> {
                        Log.w(TAG, "Unknown message type: ${json.optString("type")}")
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Failed to parse message: ${e.message}")
            }
        }

        override fun onError(conn: WebSocket?, ex: Exception) {
            Log.e(TAG, "Server error: ${ex.message}")
            postOnMain { listener.onServerError(ex.message ?: "unknown error") }
        }
    }
}
