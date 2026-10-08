package org.androidaudioplugin.hosting

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import androidx.core.content.ContextCompat
import java.util.Collections

/**
 * Watches packages being installed, updated or removed while the host process is running.
 * It marks the native installed plugins list as stale (it is re-queried on the next lookup),
 * and notifies [onInstalledPluginsChangedListeners] so that hosts can refresh their own plugin lists.
 *
 * Package broadcasts are filtered by package visibility, but hosts declare `<queries>` for
 * [AudioPluginHostHelper.AAP_ACTION_NAME], so plugin packages are visible to them.
 * The listeners may also be notified about non-plugin packages, so treat it only as a hint to re-query.
 *
 * The receiver is registered once per process by [AudioPluginClientBase] and
 * [AudioPluginClientInitializer], and stays registered for the lifetime of the process.
 */
object InstalledPluginsMonitor {
    /** Invoked on the main thread with the changed package name. */
    val onInstalledPluginsChangedListeners: MutableList<(String) -> Unit> = Collections.synchronizedList(mutableListOf())

    private var registered = false

    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            val packageName = intent.data?.schemeSpecificPart ?: return
            notifyInstalledPluginsChanged()
            onInstalledPluginsChangedListeners.toTypedArray().forEach { it(packageName) }
        }
    }

    @Synchronized
    fun register(context: Context) {
        if (registered)
            return
        val filter = IntentFilter().apply {
            addAction(Intent.ACTION_PACKAGE_ADDED)
            addAction(Intent.ACTION_PACKAGE_REPLACED)
            addAction(Intent.ACTION_PACKAGE_REMOVED)
            // a plugin service component may be enabled or disabled
            addAction(Intent.ACTION_PACKAGE_CHANGED)
            addDataScheme("package")
        }
        // Only system broadcasts are received, which are still delivered to non-exported receivers.
        ContextCompat.registerReceiver(context.applicationContext, receiver, filter, ContextCompat.RECEIVER_NOT_EXPORTED)
        registered = true
    }

    @JvmStatic
    private external fun notifyInstalledPluginsChanged()
}
