package org.androidaudiopluginframework.samples.aaplv2sample

import android.content.Intent
import android.os.IBinder
import android.util.Log
import org.androidaudiopluginframework.AudioPluginService

class AAPLV2SampleService : AudioPluginService()
{
    override fun onBind(intent: Intent?): IBinder? {
        Log.d("AAPLV2SampleService", "onBind invoked.")
        return super.onBind(intent)
    }
}
