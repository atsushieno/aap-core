package org.androidaudioplugin

import android.content.Context
import android.view.View
import android.view.ViewGroup

open class AudioPluginView(context: Context) : ViewGroup(context) {
    var view: View? = null

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
}
