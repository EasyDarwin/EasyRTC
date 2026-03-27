package cn.easyrtc.helper

import android.content.Context
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.RandomAccessFile
import java.nio.ByteBuffer

class MagicFileHelper private constructor(context: Context, directoryName: String) {

    private val appContext = context.applicationContext
    private val magicDirectory = File(appContext.getExternalFilesDir(null), directoryName)

    companion object {

        private const val TAG = "MagicFileHelper"

        @Volatile
        private var instance: MagicFileHelper? = null

        /**
         * 初始化单例（使用默认目录名 "magic"）
         */
        fun init(context: Context) {
            init(context, "magic")
        }

        /**
         * 初始化单例（指定目录名）
         */
        fun init(context: Context, directoryName: String) {
            if (instance == null) {
                synchronized(this) {
                    if (instance == null) {
                        instance = MagicFileHelper(context, directoryName)
                    }
                }
            }
        }

        /**
         * 获取单例实例（必须先调用 init 方法初始化）
         */
        fun getInstance(): MagicFileHelper {
            return instance ?: throw IllegalStateException(
                "MagicFileHelper must be initialized first. Call MagicFileHelper.init(context) or MagicFileHelper.init(context, directoryName) in your Application class."
            )
        }
    }

    init {
        // 初始化时清理指定目录下的所有文件
        clearMagicDirectory()
    }

    /**
     * 清理指定目录下的所有文件
     */
    private fun clearMagicDirectory() {
        try {
            // 确保目录存在
            if (!magicDirectory.exists()) {
                magicDirectory.mkdirs()
                Log.d(TAG, "Directory created: ${magicDirectory.absolutePath}")
                return
            }

            // 删除目录下的所有文件
            val files = magicDirectory.listFiles()
            if (files != null) {
                var deletedCount = 0
                for (file in files) {
                    if (file.isFile) {
                        if (file.delete()) {
                            deletedCount++
                            Log.d(TAG, "Deleted file: ${file.name}")
                        } else {
                            Log.w(TAG, "Failed to delete file: ${file.name}")
                        }
                    }
                }
                Log.d(TAG, "Cleared directory: $deletedCount files deleted from ${magicDirectory.name}")
            } else {
                Log.d(TAG, "Directory is empty: ${magicDirectory.name}")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error clearing directory: ${e.message}")
            e.printStackTrace()
        }
    }

    /**
     * 保存字节数组到指定目录下的文件
     */
    fun saveToFile(buffer: ByteArray, fileName: String, append: Boolean = true): Boolean {
        return try {
            val file = File(magicDirectory, fileName)

            // 确保目录存在
            if (!magicDirectory.exists()) {
                if (!magicDirectory.mkdirs()) {
                    Log.e(TAG, "Failed to create directory: ${magicDirectory.absolutePath}")
                    return false
                }
            }

            FileOutputStream(file, append).use { fos ->
                fos.write(buffer)
                fos.flush()
            }

            Log.d(TAG, "File saved successfully: ${file.absolutePath}")
            true
        } catch (e: IOException) {
            Log.e(TAG, "Error saving file: ${e.message}")
            e.printStackTrace()
            false
        }
    }


    fun saveToFile(buffer: ByteBuffer, fileName: String, append: Boolean = true): Boolean {
        return try {
            val file = File(magicDirectory, fileName)

            // 确保目录存在
            if (!magicDirectory.exists()) {
                if (!magicDirectory.mkdirs()) {
                    Log.e(TAG, "Failed to create directory: ${magicDirectory.absolutePath}")
                    return false
                }
            }

            RandomAccessFile(file, if (append) "rw" else "rw").use { raf ->
                if (append) {
                    raf.seek(raf.length()) // 移动到文件末尾
                } else {
                    raf.setLength(0) // 清空文件
                }

                val channel = raf.channel
                // 直接写入 ByteBuffer，避免内存拷贝
                channel.write(buffer)
                channel.force(true) // 强制刷新到磁盘
            }

            Log.d(TAG, "File saved successfully: ${file.absolutePath}, size: ${file.length()} bytes")
            true
        } catch (e: IOException) {
            Log.e(TAG, "Error saving file: ${e.message}")
            e.printStackTrace()
            false
        } catch (e: Exception) {
            Log.e(TAG, "Unexpected error saving file: ${e.message}")
            e.printStackTrace()
            false
        }
    }

    /**
     * 保存字节数组到指定目录
     */
    fun saveToFile(buffer: ByteArray, directory: File, fileName: String, append: Boolean = true): Boolean {
        return try {
            // 确保目录存在
            if (!directory.exists()) {
                if (!directory.mkdirs()) {
                    Log.e(TAG, "Failed to create directory: ${directory.absolutePath}")
                    return false
                }
            }

            val file = File(directory, fileName)
            FileOutputStream(file, append).use { fos ->
                fos.write(buffer)
                fos.flush()
            }

            Log.d(TAG, "File saved successfully: ${file.absolutePath}")
            true
        } catch (e: IOException) {
            Log.e(TAG, "Error saving file: ${e.message}")
            e.printStackTrace()
            false
        }
    }

    /**
     * 获取当前使用的目录
     */
    fun getCurrentDirectory(): File {
        return magicDirectory
    }

    /**
     * 获取当前目录名称
     */
    fun getCurrentDirectoryName(): String {
        return magicDirectory.name
    }

    /**
     * 检查文件是否存在于当前目录
     */
    fun fileExists(fileName: String): Boolean {
        val file = File(magicDirectory, fileName)
        return file.exists()
    }

    /**
     * 删除当前目录下的文件
     */
    fun deleteFile(fileName: String): Boolean {
        val file = File(magicDirectory, fileName)
        return if (file.exists()) {
            val result = file.delete()
            if (result) {
                Log.d(TAG, "File deleted: ${file.absolutePath}")
            }
            result
        } else {
            false
        }
    }

    /**
     * 获取当前目录下文件的完整路径
     */
    fun getFilePath(fileName: String): String {
        val file = File(magicDirectory, fileName)
        return file.absolutePath
    }

    /**
     * 手动清理当前目录（可以在需要时调用）
     */
    fun clearCurrentDirectory(): Boolean {
        return try {
            clearMagicDirectory()
            true
        } catch (e: Exception) {
            Log.e(TAG, "Error clearing directory manually: ${e.message}")
            false
        }
    }

    /**
     * 获取当前目录下的文件列表
     */
    fun getFilesInCurrentDirectory(): Array<File>? {
        return magicDirectory.listFiles { file -> file.isFile }
    }

    /**
     * 获取当前目录下的文件数量
     */
    fun getFileCountInCurrentDirectory(): Int {
        return magicDirectory.listFiles { file -> file.isFile }?.size ?: 0
    }

    /**
     * 获取当前目录的绝对路径
     */
    fun getCurrentDirectoryPath(): String {
        return magicDirectory.absolutePath
    }
}