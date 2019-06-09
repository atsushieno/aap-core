package org.androidaudiopluginframework

import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.util.Log

open class AudioPluginService : Service()
{
    companion object {
        const val NATIVE_LIBRARY_NAME = "aapnativebridge"
        const val AAP_ACTION_NAME = "org.androidaudiopluginframework.AudioPluginService"

        init {
            System.loadLibrary(NATIVE_LIBRARY_NAME)
        }

        @JvmStatic
        external fun createBinder() : IBinder
    }
    var native_binder : IBinder? = null

    override fun onBind(intent: Intent?): IBinder? {
        Log.d ("AudioPluginService", "onBind invoked")
        if (native_binder == null)
            native_binder = createBinder()
        return native_binder
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.d("AudioPluginService", "onStartCommand invoked")
        return super.onStartCommand(intent, flags, startId)
    }
}
