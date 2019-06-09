package org.androidaudiopluginframework.samples.aapbarebonesample

import android.content.Intent
import android.os.IBinder
import android.util.Log
import org.androidaudiopluginframework.AudioPluginService

class AAPBareBoneSampleService : AudioPluginService()
{
    override fun onBind(intent: Intent?): IBinder? {
        Log.d("AAPBareBoneSampleService", "onBind invoked.")
        return super.onBind(intent)
    }
}
