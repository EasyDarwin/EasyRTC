package cn.easyrtc.helper

import android.content.Context
import android.media.*
import android.os.Process
import android.util.Log
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean

class RemoteRTCAudioHelper(context: Context) {

    companion object {
        private const val TAG = "RemoteRTCAudioHelper"

        private const val SAMPLE_RATE = 8000
        private const val CHANNEL_CONFIG = AudioFormat.CHANNEL_OUT_MONO
        private const val AUDIO_FORMAT = AudioFormat.ENCODING_PCM_16BIT

        // 20ms @ 8kHz * 2bytes = 320 bytes
        private const val FRAME_SIZE = 320

        // jitter buffer（≈4秒）
        private const val MAX_QUEUE_SIZE = 200
    }

    private val audioManager =
        context.applicationContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager

    private val audioQueue = LinkedBlockingQueue<ByteArray>(MAX_QUEUE_SIZE)
    private val playing = AtomicBoolean(false)
    private val stopped = AtomicBoolean(false)

    private var audioTrack: AudioTrack? = null
    private var playbackThread: Thread? = null

    /** 静音帧（20ms） */
    private val silenceFrame = ByteArray(FRAME_SIZE)

    // ===================== 外部接口 =====================

    /**
     * 零拷贝版本（调用方保证 data 不再修改）
     */
    fun processRemoteAudioFrame(data: ByteArray, size: Int) {
        if (size <= 0) return
        ensureStarted()
        enqueue(data)
    }

    /**
     * 线程安全版本（推荐）
     */
    fun processRemoteAudioFrameSafe(data: ByteArray, size: Int) {
        if (size <= 0) return
        ensureStarted()
        enqueue(data.copyOf(size))
    }

    fun isPlaying(): Boolean = playing.get()

    fun release() {
        stopped.set(true)
        playing.set(false)

        playbackThread?.interrupt()
        playbackThread = null

        audioQueue.clear()

        try {
            audioTrack?.apply {
                stop()
                release()
            }
        } catch (e: Exception) {
            Log.e(TAG, "release error: ${e.message}")
        } finally {
            audioTrack = null
        }

        Log.d(TAG, "Audio released")
    }

    // ===================== 内部实现 =====================

    private fun ensureStarted() {
        if (playing.get()) return
        synchronized(this) {
            if (playing.get()) return
            startInternal()
        }
    }

    private fun startInternal() {
        stopped.set(false)
        setupAudioRoute()

        val minBuffer = AudioTrack.getMinBufferSize(
            SAMPLE_RATE,
            CHANNEL_CONFIG,
            AUDIO_FORMAT
        )

        val bufferSize = minBuffer * 2

        audioTrack = AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                    .build()
            )
            .setAudioFormat(
                AudioFormat.Builder()
                    .setSampleRate(SAMPLE_RATE)
                    .setChannelMask(CHANNEL_CONFIG)
                    .setEncoding(AUDIO_FORMAT)
                    .build()
            )
            .setBufferSizeInBytes(bufferSize)
            .setTransferMode(AudioTrack.MODE_STREAM)
            .build()

        audioTrack?.play()
        audioTrack?.setVolume(1.0F)

        startPlaybackThread()

        playing.set(true)

        Log.d(TAG, "Audio started, buffer=$bufferSize")
    }

    private fun startPlaybackThread() {
        playbackThread = Thread {
            Process.setThreadPriority(
                Process.THREAD_PRIORITY_AUDIO
            )

            Log.d(TAG, "Playback thread started")

            while (!stopped.get()) {
                try {
                    val frame = audioQueue.poll(0, TimeUnit.MILLISECONDS)
                    if (frame != null) {
                        writeFrame(frame)
                    } else {
                        // ★ 关键：无数据写静音
                        writeFrame(silenceFrame)
                    }
                } catch (e: InterruptedException) {
                    break
                } catch (e: Exception) {
                    Log.e(TAG, "Playback loop error: ${e.message}")
                }
            }

            Log.d(TAG, "Playback thread exit")
        }.apply {
            name = "RTC-AudioPlayback"
            start()
        }
    }

    private fun writeFrame(data: ByteArray) {
        val track = audioTrack ?: return

        var offset = 0
        var remaining = data.size

        while (remaining > 0 && !stopped.get()) {
            val written = track.write(
                data,
                offset,
                remaining,
                AudioTrack.WRITE_BLOCKING
            )

            if (written > 0) {
                offset += written
                remaining -= written
            } else {
                Log.e(TAG, "AudioTrack write error=$written")
                break
            }
        }
    }

    private fun enqueue(data: ByteArray) {
        if (!audioQueue.offer(data)) {
            audioQueue.poll()
            audioQueue.offer(data)
            Log.w(TAG, "Audio queue overflow, drop old frame")
        }
    }

    /**
     * 建议只调用一次，或放在 Activity
     */
    private fun setupAudioRoute() {
        try {
            audioManager.isSpeakerphoneOn = true
//            audioManager.mode = AudioManager.MODE_IN_COMMUNICATION
            audioManager.mode = AudioManager.MODE_NORMAL
        } catch (e: Exception) {
            Log.e(TAG, "Audio route error: ${e.message}")
        }
    }
}
