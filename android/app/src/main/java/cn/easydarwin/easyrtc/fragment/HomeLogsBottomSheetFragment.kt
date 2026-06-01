package cn.easydarwin.easyrtc.fragment

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.recyclerview.widget.DividerItemDecoration
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.google.android.material.button.MaterialButton
import cn.easydarwin.easyrtc.R
import cn.easydarwin.easyrtc.utils.CallLog

class HomeLogsBottomSheetFragment : BottomSheetDialogFragment() {

    private val handler = Handler(Looper.getMainLooper())
    private var rvLogs: RecyclerView? = null
    private var etMessage: EditText? = null
    private var buttonSend: MaterialButton? = null
    private val adapter = LogAdapter()
    private var lastEntryCount = 0

    private val refreshRunnable = object : Runnable {
        override fun run() {
            val entries = CallLog.entries()
            val newCount = entries.size - lastEntryCount
            val hasNew = newCount > 0
            lastEntryCount = entries.size

            val lm = rvLogs?.layoutManager as? LinearLayoutManager
            val lastVisible = lm?.findLastCompletelyVisibleItemPosition() ?: -1
            val wasAtBottom = lastVisible >= adapter.itemCount - 1

            if (hasNew) {
                val insertStart = adapter.itemCount
                adapter.entries = entries
                adapter.notifyItemRangeInserted(insertStart, newCount)
            }

            if (hasNew && wasAtBottom && adapter.itemCount > 0) {
                rvLogs?.scrollToPosition(adapter.itemCount - 1)
            }
            handler.postDelayed(this, 500)
        }
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return inflater.inflate(R.layout.fragment_home_logs_sheet, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        rvLogs = view.findViewById(R.id.rv_logs)
        etMessage = view.findViewById(R.id.et_msg_sheet)
        buttonSend = view.findViewById(R.id.button_send_sheet)

        rvLogs?.layoutManager = LinearLayoutManager(requireContext())
        rvLogs?.addItemDecoration(DividerItemDecoration(requireContext(), LinearLayoutManager.VERTICAL))
        rvLogs?.adapter = adapter

        buttonSend?.setOnClickListener {
            val text = etMessage?.text?.toString().orEmpty()
            (parentFragment as? HomeFragment)?.sendTextMessage(text)
            etMessage?.setText("")
        }
    }

    override fun onStart() {
        super.onStart()
        dialog?.findViewById<View>(com.google.android.material.R.id.design_bottom_sheet)?.let { sheet ->
            BottomSheetBehavior.from(sheet).state = BottomSheetBehavior.STATE_EXPANDED
        }
        handler.post(refreshRunnable)
    }

    override fun onStop() {
        handler.removeCallbacks(refreshRunnable)
        super.onStop()
    }

    override fun onDestroyView() {
        handler.removeCallbacks(refreshRunnable)
        rvLogs = null
        etMessage = null
        buttonSend = null
        super.onDestroyView()
    }

    private inner class LogAdapter : RecyclerView.Adapter<LogAdapter.ViewHolder>() {

        var entries: List<String> = emptyList()

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val tv = LayoutInflater.from(parent.context)
                .inflate(R.layout.item_call_log, parent, false) as TextView
            return ViewHolder(tv)
        }

        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            holder.textView.text = entries[position]
        }

        override fun getItemCount() = entries.size

        inner class ViewHolder(val textView: TextView) : RecyclerView.ViewHolder(textView) {
            init {
                textView.setOnLongClickListener {
                    val text = entries[adapterPosition]
                    val clipboard = requireContext().getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                    clipboard.setPrimaryClip(ClipData.newPlainText("call log", text))
                    Toast.makeText(requireContext(), "已复制", Toast.LENGTH_SHORT).show()
                    true
                }
            }
        }
    }
}
