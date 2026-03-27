package cn.easydarwin.easyrtc.utils

import android.util.Base64
import java.security.MessageDigest
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

class CryptoUtils {

    companion object {

        /**
         * SHA256 哈希
         */
        fun sha256Hash(input: String): String {
            return hashString(input, "SHA-256")
        }

        /**
         * SHA256 哈希返回字节数组
         */
        fun sha256Bytes(input: String): ByteArray {
            return try {
                val digest = MessageDigest.getInstance("SHA-256")
                digest.digest(input.toByteArray(Charsets.UTF_8))
            } catch (e: Exception) {
                e.printStackTrace()
                byteArrayOf()
            }
        }

        /**
         * SHA256 哈希返回 Base64 字符串
         */
        fun sha256Base64(input: String): String {
            return try {
                val hash = sha256Bytes(input)
                Base64.encodeToString(hash, Base64.NO_WRAP)
            } catch (e: Exception) {
                e.printStackTrace()
                ""
            }
        }

        /**
         * 通用哈希方法
         */
        private fun hashString(input: String, algorithm: String): String {
            return try {
                val digest = MessageDigest.getInstance(algorithm)
                val hash = digest.digest(input.toByteArray(Charsets.UTF_8))
                val hexString = StringBuilder()

                for (byte in hash) {
                    val hex = Integer.toHexString(0xff and byte.toInt())
                    if (hex.length == 1) {
                        hexString.append('0')
                    }
                    hexString.append(hex)
                }

                hexString.toString()
            } catch (e: Exception) {
                e.printStackTrace()
                ""
            }
        }

        /**
         * 加盐哈希
         */
        fun sha256WithSalt(input: String, salt: String): String {
            return sha256Hash(input + salt)
        }

        /**
         * HMAC-SHA256 签名
         */
        fun hmacSha256(data: String, key: String): String {
            return try {
                val algorithm = "HmacSHA256"
                val mac = Mac.getInstance(algorithm)
                val secretKey = SecretKeySpec(key.toByteArray(Charsets.UTF_8), algorithm)
                mac.init(secretKey)
                val hash = mac.doFinal(data.toByteArray(Charsets.UTF_8))
                bytesToHex(hash)
            } catch (e: Exception) {
                e.printStackTrace()
                ""
            }
        }

        /**
         * 字节数组转十六进制
         */
        private fun bytesToHex(bytes: ByteArray): String {
            return bytes.joinToString("") { "%02x".format(it) }
        }
    }
}