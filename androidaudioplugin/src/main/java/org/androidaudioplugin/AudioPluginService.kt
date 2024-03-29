package org.androidaudioplugin

import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.content.Context
import android.os.Looper

/**
 * The Audio plugin service class. It should not be derived. We check the service class name strictly.
 *
 * Every AAP should implement AudioPluginInterface.aidl, in native code.
 *
 * Typical user developers don't have to write any code to create their own plugins in Kotlin code.
 * They are implemented in native code to get closest to realtime audio processing.
 */
open class AudioPluginService : Service()
{
    companion object {
        private var initialized = false
        private var waitForDebugger = false

        fun initialize(context: Context) {
            if (initialized)
                return
            if (waitForDebugger) {
                android.os.Debug.waitForDebugger()
                waitForDebugger = true
            }
            AudioPluginNatives.initializeAAPJni(context)
            initialized = true
        }
    }

    /**
     * This interface is used by AAP Service extensions that will be initialized at onCreate().
     * Not to be confused with AAP extension in the native (audio plugin) API context (`get_extension()`).
     *
     * By registering this extension as a `<meta-data>` element in AndroidManifest.xml, whose
     * `android:name` is `org.androidaudioplugin.AudioPluginService#Extensions`, the implementation
     * class that is specified in the `android:value` attribute of the `<meta-data>` element
     * will be instantiated and those members are invoked, wherever appropriate.
     */
    interface Extension {
        fun initialize(ctx: Context)
        fun cleanup()
    }

    private var nativeBinder : IBinder? = null
    var extensions = mutableListOf<Extension>()

    override fun onCreate() {
        initialize(this)

        val si = AudioPluginServiceHelper.getLocalAudioPluginService(this)
        si.extensions.forEach { e ->
            if(e == "")
                return@forEach
            val c = Class.forName(e)
            val ext = c.newInstance() as Extension
            ext.initialize(this)
            extensions.add(ext)
        }
    }

    private var eventProcessorLooper: Looper? = null

    override fun onBind(intent: Intent?): IBinder? {
        val existing = nativeBinder
        if (existing != null)
            AudioPluginNatives.destroyBinderForService(existing)
        nativeBinder = AudioPluginNatives.createBinderForService()

        // no need to worry about the Looper retaining; it will be released at onUnbind()
        Thread {
            AudioPluginNatives.prepareNativeLooper()
            eventProcessorLooper = Looper.myLooper()
            AudioPluginNatives.startNativeLooper()
        }.apply {
            this.contextClassLoader = contextClassLoader
            this.start()
        }

        return nativeBinder
    }

    override fun onUnbind(intent: Intent?): Boolean {
        for(ext in extensions)
            ext.cleanup()

        AudioPluginNatives.stopNativeLooper()
        if (eventProcessorLooper?.thread?.isAlive == true)
            eventProcessorLooper?.quitSafely()
        eventProcessorLooper = null

        val binder = nativeBinder
        if (binder != null)
            AudioPluginNatives.destroyBinderForService(binder)
        nativeBinder = null
        return true
    }
}
