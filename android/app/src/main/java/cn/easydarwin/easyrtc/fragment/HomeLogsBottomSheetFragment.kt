package cn.easydarwin.easyrtc.fragment

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.method.ScrollingMovementMethod
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.TextView
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.google.android.material.button.MaterialButton
import cn.easydarwin.easyrtc.R
import cn.easydarwin.easyrtc.utils.AppLogStore

class HomeLogsBottomSheetFragment : BottomSheetDialogFragment() {

    private val handler = Handler(Looper.getMainLooper())
    private var tvLogs: TextView? = null
    private var etMessage: EditText? = null
    private var buttonSend: MaterialButton? = null

    private val refreshRunnable = object : Runnable {
        override fun run() {
            tvLogs?.text = AppLogStore.text()
            handler.postDelayed(this, 500)
        }
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return inflater.inflate(R.layout.fragment_home_logs_sheet, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        tvLogs = view.findViewById(R.id.tv_logs_sheet)
        etMessage = view.findViewById(R.id.et_msg_sheet)
        buttonSend = view.findViewById(R.id.button_send_sheet)

        tvLogs?.text = AppLogStore.text()
        tvLogs?.movementMethod = ScrollingMovementMethod()
        buttonSend?.setOnClickListener {
            val text = etMessage?.text?.toString().orEmpty()
            (parentFragment as? HomeFragment)?.sendTextMessage(text)
            etMessage?.setText("")
            tvLogs?.text = AppLogStore.text()
        }
    }

    override fun onStart() {
        super.onStart()
        handler.post(refreshRunnable)
    }

    override fun onStop() {
        handler.removeCallbacks(refreshRunnable)
        super.onStop()
    }

    override fun onDestroyView() {
        handler.removeCallbacks(refreshRunnable)
        tvLogs = null
        etMessage = null
        buttonSend = null
        super.onDestroyView()
    }
}
