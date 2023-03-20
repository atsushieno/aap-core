package org.androidaudioplugin

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Color
import android.graphics.PixelFormat
import android.os.Build
import android.provider.Settings
import android.util.Log
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.TextView

// FIXME: it should register any activity lifecycle callback events to hide the overlay plugin GUI
//  if it is shown. Otherwise it will stay on top forever!
class AudioPluginView(context: Context) : ViewGroup(context) {

    private var xBeforeDrag = 0
    private var yBeforeDrag = 0
    private var dragStartedX: Float = 0f
    private var dragStartedY: Float = 0f

    class TitleBar(context: Context) : TextView(context) {
        @SuppressLint("ClickableViewAccessibility") // it matters only for visual position, TalkBack would not matter.
        override fun onTouchEvent(event: MotionEvent?): Boolean {
            if (event == null)
                return super.onTouchEvent(null)
            val owner = (parent as AudioPluginView?)
                ?: return super.onTouchEvent(event)
            when (event.action) {
                MotionEvent.ACTION_DOWN -> {
                    owner.xBeforeDrag = owner.wmLayoutParams.x
                    owner.yBeforeDrag = owner.wmLayoutParams.y
                    owner.dragStartedX = event.rawX
                    owner.dragStartedY = event.rawY
                    return true
                }
                MotionEvent.ACTION_MOVE -> {
                    owner.wmLayoutParams.x = owner.xBeforeDrag + (event.rawX - owner.dragStartedX).toInt()
                    owner.wmLayoutParams.y = owner.yBeforeDrag + (event.rawY - owner.dragStartedY).toInt()
                    owner.windowManager.updateViewLayout(owner, owner.wmLayoutParams)
                    return true
                }
            }
            return super.onTouchEvent(event)
        }
    }

    @SuppressLint("SetTextI18n")
    private val titleBar = TitleBar(context).apply {
        text = "AAP native UI"
        height = 120
        width = LayoutParams.MATCH_PARENT
        setTextColor(Color.BLACK)
        textSize = 16f
        setBackgroundColor(Color.GRAY)
        alpha = 0.7f
    }

    var view: View? = null

    @SuppressLint("ClickableViewAccessibility") // can I support TalkBack to detect "clicking outside it" ?
    override fun onTouchEvent(event: MotionEvent?): Boolean {
        val view = this.view ?: return super.onTouchEvent(event)
        return if (event == null) super.onTouchEvent(null)
        else if (event.action == MotionEvent.ACTION_DOWN || event.action == MotionEvent.ACTION_OUTSIDE) {
            val arr = IntArray(2)
            view.getLocationOnScreen(arr)
            val minX = arr[0]
            val minY = arr[1]
            if (event.x < minX || minX + view.width < event.x ||
                event.y < minY || minY + view.height < event.y)
                hideOverlay(true)
            false
        } else
            super.onTouchEvent(event)
    }

    override fun onViewAdded(child: View?) {
        if (child == titleBar)
            return // irrelevant
        if (view != null)
            throw AudioPluginException("child View is already added to this AudioPluginView")
        if (child != null)
            view = child
        addView(titleBar)
        super.onViewAdded(child)
    }

    override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {
        if (!changed)
            return
        if (view != null) {
            titleBar.layout(l, t, r, t + 60)
            view!!.layout(l, t + 60, r, b)
        }
    }

    private var overlayState = false

    private val wmLayoutParams = WindowManager.LayoutParams(
        width,
        height,
        WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
        WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH or WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL,
        PixelFormat.TRANSLUCENT)
    val windowManager = context.getSystemService(Context.WINDOW_SERVICE) as WindowManager

    fun showOverlay() {
        if (overlayState)
            return
        if (!Settings.canDrawOverlays(context)) {
            Log.w("AAP", "SYSTEM_ALERT_WINDOW permission is not granted. Unable to show the plugin UI")
            return
        }
        val width = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) { windowManager.currentWindowMetrics.bounds.width() * 0.8 } else 400.0
        val height = width * 0.8
        wmLayoutParams.width = width.toInt()
        wmLayoutParams.height = height.toInt()
        windowManager.addView(this, wmLayoutParams)
        overlayState = true
    }

    fun hideOverlay(shouldNotifyHideEvent: Boolean) {
        if (!overlayState)
            return
        val wm = context.getSystemService(Context.WINDOW_SERVICE) as WindowManager
        wm.removeView(this)
        overlayState = false
        if (shouldNotifyHideEvent) {
            // FIXME: notify "overlay window closed" event to host
        }
    }
}
