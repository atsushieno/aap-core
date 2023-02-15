package org.androidaudioplugin

import android.content.Context
import android.graphics.PixelFormat
import android.os.Build
import android.provider.Settings
import android.util.Log
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager

// FIXME: it should register any activity lifecycle callback events to hide the overlay plugin GUI
//  if it is shown. Otherwise it will stay on top forever!
class AudioPluginView(context: Context) : ViewGroup(context) {
    var view: View? = null

    override fun onTouchEvent(event: MotionEvent?): Boolean {
        val view = this.view
        return if (view != null && event != null &&
            (event.action == MotionEvent.ACTION_DOWN || event.action == MotionEvent.ACTION_OUTSIDE)) {
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
        if (view != null)
            throw AudioPluginException("child View is already added to this AudioPluginView")
        if (child != null)
            view = child
        super.onViewAdded(child)
    }

    override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {
        if (view != null)
            view!!.layout(l, t, r, b)
    }

    var overlayState = false

    fun showOverlay() {
        if (overlayState)
            return
        if (!Settings.canDrawOverlays(context)) {
            Log.w("AAP", "SYSTEM_ALERT_WINDOW permission is not granted. Unable to show the plugin UI")
            return
        }
        val wm = context.getSystemService(Context.WINDOW_SERVICE) as WindowManager
        val width = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) { wm.currentWindowMetrics.bounds.width() * 0.8 } else 400.0
        val height = width * 0.7
        val p = WindowManager.LayoutParams(
            width.toInt(),
            height.toInt(),
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH or WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL,
            PixelFormat.TRANSLUCENT)
        wm.addView(this, p)
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
