package org.androidaudiopluginframework.samples.aapbarebonesample

import android.app.Service
import android.content.Intent
import android.os.IBinder

class AndroidAudioPluginService : Service()
{
    companion object {
        init {
            System.loadLibrary("aapnativebridge")
        }
    }
    var native_binder : IBinder? = null

    override fun onBind(intent: Intent?): IBinder? {
        if (native_binder == null)
            native_binder = createBinder()
        return native_binder
    }

    external fun createBinder() : IBinder
}
