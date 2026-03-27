package cn.easyrtc.helper

import android.annotation.SuppressLint
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.media.audiofx.AcousticEchoCanceler
import android.media.audiofx.AutomaticGainControl
import android.media.audiofx.NoiseSuppressor
import kotlinx.coroutines.*
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.coroutines.coroutineContext

class AudioHelper {

    companion object {
        private const val SAMPLE_RATE = 8000
        private const val CHANNEL_CONFIG = AudioFormat.CHANNEL_IN_MONO
        private const val AUDIO_FORMAT = AudioFormat.ENCODING_PCM_16BIT
        private const val READ_AUDIO_BUFFER_SIZE = 320 // 20ms
        private var aec: AcousticEchoCanceler? = null
        private var ns: NoiseSuppressor? = null
        private var agc: AutomaticGainControl? = null
    }

    private var audioRecord: AudioRecord? = null
    private val isRecording = AtomicBoolean(false)
    private var recordingJob: Job? = null

    interface OnAudioDataListener {
        fun onAudioData(data: ByteArray, pts: Long)
        fun onError(error: String)
    }

    private var audioDataListener: OnAudioDataListener? = null

    fun setOnAudioDataListener(listener: OnAudioDataListener) {
        audioDataListener = listener
    }

    @SuppressLint("MissingPermission")
    fun start(): Boolean {
        if (isRecording.get()) return false

        try {
            val minBufferSize = AudioRecord.getMinBufferSize(
                SAMPLE_RATE, CHANNEL_CONFIG, AUDIO_FORMAT
            )

            if (minBufferSize == AudioRecord.ERROR || minBufferSize == AudioRecord.ERROR_BAD_VALUE) {
                audioDataListener?.onError("Invalid buffer size")
                return false
            }


            audioRecord = AudioRecord(
//                MediaRecorder.AudioSource.VOICE_COMMUNICATION, // 使用通信音频源，系统会做一些基础处理
                MediaRecorder.AudioSource.VOICE_RECOGNITION, //VOICE_RECOGNITION 很多时候比 VOICE_COMMUNICATION 更纯净：
                SAMPLE_RATE, CHANNEL_CONFIG, AUDIO_FORMAT, minBufferSize * 2
            )

            if (audioRecord?.state != AudioRecord.STATE_INITIALIZED) {
                audioDataListener?.onError("AudioRecord initialization failed")
                return false
            }

            audioRecord?.startRecording()

            enableAudioEffects()

            if (audioRecord?.recordingState != AudioRecord.RECORDSTATE_RECORDING) {
                audioDataListener?.onError("Failed to start recording")
                return false
            }

            isRecording.set(true)
            startRecordingLoop()
            return true

        } catch (e: Exception) {
            audioDataListener?.onError("Start failed: ${e.message}")
            return false
        }
    }


    private fun enableAudioEffects() {
        audioRecord?.audioSessionId?.let { sessionId ->

            if (AcousticEchoCanceler.isAvailable()) {
                aec = AcousticEchoCanceler.create(sessionId)
                aec?.enabled = true
            }

            if (NoiseSuppressor.isAvailable()) {
                ns = NoiseSuppressor.create(sessionId)
                ns?.enabled = true
            }

            if (AutomaticGainControl.isAvailable()) {
                agc = AutomaticGainControl.create(sessionId)
                agc?.enabled = true
            }
        }
    }

    fun stop() {
        if (!isRecording.getAndSet(false)) return

        recordingJob?.cancel()
        recordingJob = null

        try {
            audioRecord?.let {
                if (it.recordingState == AudioRecord.RECORDSTATE_RECORDING) {
                    it.stop()
                }
                it.release()
            }
        } catch (e: Exception) {
            audioDataListener?.onError("Stop error: ${e.message}")
        } finally {
            audioRecord = null
        }
    }

    private fun startRecordingLoop() {
        recordingJob = CoroutineScope(Dispatchers.IO).launch {
            recordAudio()
        }
    }

    private suspend fun recordAudio() {
        val buffer = ByteArray(READ_AUDIO_BUFFER_SIZE)

        while (isRecording.get() && coroutineContext.isActive) {
            try {
                val readResult = audioRecord?.read(buffer, 0, READ_AUDIO_BUFFER_SIZE) ?: 0

                when {
                    readResult > 0 -> {
                        // 直接传递原始数据，不做任何处理
                        val audioData = buffer.copyOf(readResult)
                        audioDataListener?.onAudioData(audioData, System.currentTimeMillis())
                    }

                    readResult == AudioRecord.ERROR_INVALID_OPERATION -> {
                        audioDataListener?.onError("Invalid operation")
                        break
                    }

                    readResult == AudioRecord.ERROR_BAD_VALUE -> {
                        audioDataListener?.onError("Bad value")
                        break
                    }

                    readResult == AudioRecord.ERROR_DEAD_OBJECT -> {
                        audioDataListener?.onError("Dead object")
                        break
                    }

                    else -> {
                        delay(1) // 无数据时短暂等待
                    }
                }
            } catch (e: Exception) {
                if (isRecording.get()) {
                    audioDataListener?.onError("Record error: ${e.message}")
                    delay(10)
                }
            }
        }
    }

    fun isRecording(): Boolean = isRecording.get()
}