package cn.easydarwin.easyrtc.view

import android.app.Activity
import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.TextView
import androidx.constraintlayout.widget.ConstraintLayout
import cn.easydarwin.easyrtc.R

class TitleBarView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : ConstraintLayout(context, attrs) {

    private val ivBack: ImageView
    private val tvTitle: TextView
    private val rightContainer: FrameLayout

    init {
        LayoutInflater.from(context).inflate(R.layout.view_title_bar, this, true)
        ivBack = findViewById(R.id.ivBack)
        tvTitle = findViewById(R.id.tvTitle)
        rightContainer = findViewById(R.id.rightContainer)

        ivBack.setOnClickListener {
            if (context is Activity) {
                context.finish()
            }
        }
    }

    fun setTitle(title: String) {
        tvTitle.text = title
    }

    fun showBack(show: Boolean) {
        ivBack.visibility = if (show) VISIBLE else INVISIBLE
    }

    fun setRightView(view: android.view.View?) {
        rightContainer.removeAllViews()
        if (view != null) {
            rightContainer.addView(view)
        }
    }

    fun setTitleBarBackgroundColor(color: Int) {
        setBackgroundColor(color)
    }
}
