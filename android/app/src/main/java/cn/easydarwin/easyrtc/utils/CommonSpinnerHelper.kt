// CommonSpinnerHelper.kt
package cn.easydarwin.easyrtc.utils

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.*
import androidx.core.content.ContextCompat
import cn.easydarwin.easyrtc.R

object CommonSpinnerHelper {

    // 存储每个下拉选的当前选中位置
    private val selectionMap = mutableMapOf<Int, Int>()

    /**
     * 创建下拉选择器
     * @param context 上下文
     * @param anchorView 锚点视图（点击触发下拉的视图）
     * @param displayView 显示选中项的TextView
     * @param dataArray 数据数组
     * @param defaultPosition 默认选中位置
     * @param onItemSelected 选中回调
     */
    fun createSpinner(
        context: Context,
        anchorView: View,
        displayView: TextView,
        dataArray: Array<String>,
        defaultPosition: Int = 0,
        onItemSelected: (selectedValue: String, position: Int) -> Unit
    ) {
        val viewId = anchorView.id

        // 初始化选中位置
        if (!selectionMap.containsKey(viewId)) {
            selectionMap[viewId] = defaultPosition
        }

        // 设置默认显示
        val currentPosition = selectionMap[viewId] ?: defaultPosition
        if (dataArray.isNotEmpty() && currentPosition < dataArray.size) {
            displayView.text = dataArray[currentPosition]
        }

        anchorView.setOnClickListener {
            // 每次点击时获取最新的选中位置
            val latestPosition = selectionMap[viewId] ?: defaultPosition
            showSpinnerPopup(context, anchorView, displayView, dataArray, latestPosition, onItemSelected)
        }
    }

    /**
     * 通过资源ID创建下拉选择器
     */
    fun initSpinner(
        context: Context,
        anchorView: View,
        displayView: TextView,
        stringArrayResId: Int,
        defaultPosition: Int = 0,
        onItemSelected: (selectedValue: String, position: Int) -> Unit
    ) {
        val dataArray = context.resources.getStringArray(stringArrayResId)
        createSpinner(context, anchorView, displayView, dataArray, defaultPosition, onItemSelected)
    }

    /**
     * 显示下拉弹窗
     */
    private fun showSpinnerPopup(
        context: Context,
        anchorView: View,
        displayView: TextView,
        dataArray: Array<String>,
        currentPosition: Int,
        onItemSelected: (selectedValue: String, position: Int) -> Unit
    ) {
        val popupView = ListView(context)
        val viewId = anchorView.id

        // 创建自定义适配器，支持选中高亮
        val adapter = object : ArrayAdapter<String>(context, R.layout.spinner_item, dataArray) {
            override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
                val view = convertView ?: LayoutInflater.from(context).inflate(R.layout.spinner_item, parent, false)
                val textView = view.findViewById<TextView>(R.id.tv_item)
                textView.text = getItem(position)

                // 设置选中状态背景
                if (position == currentPosition) {
                    view.setBackgroundColor(ContextCompat.getColor(context, R.color.selected_item_background))
                    textView.setTextColor(ContextCompat.getColor(context, R.color.selected_item_text))
                } else {
                    view.setBackgroundColor(ContextCompat.getColor(context, R.color.white))
                    textView.setTextColor(ContextCompat.getColor(context, R.color.normal_item_text))
                }

                return view
            }
        }

        popupView.adapter = adapter
        popupView.choiceMode = ListView.CHOICE_MODE_SINGLE
        popupView.setItemChecked(currentPosition, true)

        // 确保滚动到选中项
        popupView.post {
            popupView.setSelection(currentPosition)
        }

        // 设置列表选择器（点击时的效果）
        popupView.selector = ContextCompat.getDrawable(context, R.drawable.list_selector)

        // 设置固定高度，避免显示问题
        val itemHeight = context.resources.getDimensionPixelSize(R.dimen.spinner_item_height)
        val maxHeight = context.resources.getDimensionPixelSize(R.dimen.spinner_max_height)
        val calculatedHeight = (itemHeight * dataArray.size).coerceAtMost(maxHeight)

        val popupWindow = PopupWindow(
            popupView,
            anchorView.width,
            calculatedHeight,
            true
        )

        // 设置弹窗背景
        popupWindow.setBackgroundDrawable(
            ContextCompat.getDrawable(context, R.drawable.popup_background)
        )
        popupWindow.elevation = 8f
        popupWindow.isFocusable = true

        // 列表项点击事件
        popupView.onItemClickListener = AdapterView.OnItemClickListener { _, _, position, _ ->
            val selectedValue = dataArray[position]
            displayView.text = selectedValue
            selectionMap[viewId] = position // 更新选中位置
            onItemSelected(selectedValue, position)
            popupWindow.dismiss()
        }

        // 显示在锚点视图下方
        popupWindow.showAsDropDown(anchorView)
    }

    /**
     * 手动设置选中位置（用于外部更新）
     */
    fun setSelection(anchorView: View, position: Int, displayView: TextView? = null, dataArray: Array<String>? = null) {
        val viewId = anchorView.id
        selectionMap[viewId] = position
        // 更新显示文本
        if (displayView != null && dataArray != null && position < dataArray.size) {
            displayView.text = dataArray[position]
        }
    }

    /**
     * 通过资源手动设置选中位置
     */
    fun setSelectionByResource(context: Context, anchorView: View, position: Int, displayView: TextView? = null, stringArrayResId: Int? = null) {
        val viewId = anchorView.id
        selectionMap[viewId] = position

        // 更新显示文本
        if (displayView != null && stringArrayResId != null) {
            val dataArray = context.resources.getStringArray(stringArrayResId)
            if (position < dataArray.size) {
                displayView.text = dataArray[position]
            }
        }
    }

    /**
     * 设置默认选中项
     */
    fun setDefaultSelection(displayView: TextView, dataArray: Array<String>, defaultPosition: Int = 0) {
        if (dataArray.isNotEmpty() && defaultPosition < dataArray.size) {
            displayView.text = dataArray[defaultPosition]
        }
    }

    /**
     * 通过资源设置默认选中项
     */
    fun setDefaultSelectionByResource(
        context: Context,
        displayView: TextView,
        stringArrayResId: Int,
        defaultPosition: Int = 0
    ) {
        val dataArray = context.resources.getStringArray(stringArrayResId)
        setDefaultSelection(displayView, dataArray, defaultPosition)
    }

    /**
     * 获取当前选中位置
     */
    fun getCurrentPosition(anchorView: View): Int {
        return selectionMap[anchorView.id] ?: 0
    }

    /**
     * 获取当前选中值
     */
    fun getCurrentValue(context: Context, anchorView: View, stringArrayResId: Int): String {
        val dataArray = context.resources.getStringArray(stringArrayResId)
        val position = getCurrentPosition(anchorView)
        return if (position < dataArray.size) dataArray[position] else ""
    }

    /**
     * 清除所有选择记录（在Fragment销毁时调用）
     */
    fun clearSelections() {
        selectionMap.clear()
    }
}