package org.androidaudiopluginframework.samples.aapbarebonesample

import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.util.Log

class AndroidAudioPluginService : Service()
{
    companion object {
        const val NATIVE_LIBRARY_NAME = "aapnativebridge"
        const val AAP_ACTION_NAME = "org.androidaudiopluginframework.AndroidAudioPluginService"

        init {
            System.loadLibrary(NATIVE_LIBRARY_NAME)
        }

        @JvmStatic
        external fun createBinder() : IBinder
    }
    var native_binder : IBinder? = null

    override fun onBind(intent: Intent?): IBinder? {
        Log.d ("AndroidAudioPluginService", "onBind invoked")
        if (native_binder == null)
            native_binder = createBinder()
        return native_binder
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.d("AndroidAudioPluginService", "onStartCommand invoked")
        return super.onStartCommand(intent, flags, startId)
    }
}
