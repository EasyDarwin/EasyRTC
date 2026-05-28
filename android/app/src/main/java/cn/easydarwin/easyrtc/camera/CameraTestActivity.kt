package cn.easydarwin.easyrtc.camera

import android.os.Bundle
import android.view.TextureView
import android.widget.FrameLayout
import androidx.appcompat.app.AppCompatActivity
import java.util.concurrent.CountDownLatch

class CameraTestActivity : AppCompatActivity() {

    lateinit var textureView: TextureView
    var surfaceLatch = CountDownLatch(1)
    private var surfaceTexture: android.graphics.SurfaceTexture? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        textureView = TextureView(this)
        textureView.layoutParams = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        )

        val root = FrameLayout(this)
        root.layoutParams = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        )
        root.addView(textureView)
        setContentView(root)

        textureView.surfaceTextureListener = object : TextureView.SurfaceTextureListener {
            override fun onSurfaceTextureAvailable(st: android.graphics.SurfaceTexture, w: Int, h: Int) {
                surfaceTexture = st
                st.setDefaultBufferSize(1920, 1080)
                surfaceLatch.countDown()
            }
            override fun onSurfaceTextureSizeChanged(st: android.graphics.SurfaceTexture, w: Int, h: Int) {}
            override fun onSurfaceTextureDestroyed(st: android.graphics.SurfaceTexture): Boolean = true
            override fun onSurfaceTextureUpdated(st: android.graphics.SurfaceTexture) {}
        }
    }
    fun acquirePreviewSurface(): android.view.Surface {
        val st = surfaceTexture ?: throw IllegalStateException("SurfaceTexture not available")
        return android.view.Surface(st)
    }
}
