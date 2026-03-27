package cn.easyrtc.helper

import android.media.MediaCodecInfo
import android.media.MediaCodecList

object MediaHelper{
    // 码率配置（单位：bps）
    const val BITRATE_480P = 1000000L   // 1 Mbps for 480p
    const val BITRATE_720P = 2000000L   // 2 Mbps for 720p
    const val BITRATE_1080P = 4000000L  // 4 Mbps for 1080p
    const val BITRATE_2K = 8000000L     // 8 Mbps for 2K
    const val BITRATE_4K = 20000000L    // 20 Mbps for 4K

    class CodecInfo {
        var mName: String? = null
        var mColorFormat: Int = 0
    }

    fun getRecommendedBitrate(width: Int, height: Int, frameRate: Int = 30): Int {
        val pixelCount = width * height
        return when {
            pixelCount <= 640 * 480 -> BITRATE_480P.toInt()
            pixelCount <= 1280 * 720 -> BITRATE_720P.toInt()
            pixelCount <= 1920 * 1080 -> (BITRATE_1080P * frameRate / 30).toInt()
            pixelCount <= 2560 * 1440 -> (BITRATE_2K * frameRate / 30).toInt()
            else -> (BITRATE_4K * frameRate / 30).toInt()
        }
    }


    fun getListEncoders(mime: String?): ArrayList<CodecInfo> {
        // 可能有多个编码库，都获取一下
        val codecInfoList: ArrayList<CodecInfo> = ArrayList<CodecInfo>()
        val numCodecs = MediaCodecList.getCodecCount()

        for (i1 in 0..<numCodecs) {
            val codecInfo = MediaCodecList.getCodecInfoAt(i1)

            if (!codecInfo.isEncoder()) {
                continue
            }

            if (codecMatch(mime, codecInfo)) {
                val name = codecInfo.getName()
                val colorFormat: Int = getColorFormat(codecInfo, mime)

                if (colorFormat != 0) {
                    val ci = CodecInfo()
                    ci.mName = name
                    ci.mColorFormat = colorFormat
                    codecInfoList.add(ci)
                }
            }
        }

        return codecInfoList
    }

    /* ============================== private method ============================== */
    private fun codecMatch(mimeType: String?, codecInfo: MediaCodecInfo): Boolean {
        val types = codecInfo.getSupportedTypes()

        for (type in types) {
            if (type.equals(mimeType, ignoreCase = true)) {
                return true
            }
        }

        return false
    }

    private fun getColorFormat(codecInfo: MediaCodecInfo, mimeType: String?): Int {
        // 在ByteBuffer模式下，视频缓冲区根据其颜色格式进行布局。
        val capabilities = codecInfo.getCapabilitiesForType(mimeType)
        val cf = IntArray(capabilities.colorFormats.size)
        System.arraycopy(capabilities.colorFormats, 0, cf, 0, cf.size)
        val sets: MutableList<Int?> = java.util.ArrayList<Int?>()

        for (i in cf.indices) sets.add(cf[i])

        if (sets.contains(MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar)) {
            return MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar
        } else if (sets.contains(MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Planar)) {
            return MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Planar
        } else if (sets.contains(MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420PackedPlanar)) {
            return MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420PackedPlanar
        } else if (sets.contains(MediaCodecInfo.CodecCapabilities.COLOR_TI_FormatYUV420PackedSemiPlanar)) {
            return MediaCodecInfo.CodecCapabilities.COLOR_TI_FormatYUV420PackedSemiPlanar
        }

        return 0
    }
}