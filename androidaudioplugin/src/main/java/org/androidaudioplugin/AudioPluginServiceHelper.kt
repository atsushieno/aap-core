package org.androidaudioplugin

import android.content.ComponentName
import android.content.Context
import android.content.pm.PackageManager
import android.util.Size
import android.view.View
import org.androidaudioplugin.hosting.AudioPluginHostHelper

// It is used only by AudioPluginService and some plugin extensions (such as AudioPluginLv2ServiceExtension) to process client (plugin host) requests.
object AudioPluginServiceHelper {
    private val currentInstanceId = ThreadLocal<Int?>()

    @Deprecated("A plugin package may have more than one AudioPluginService. Use getLocalAudioPluginServices(), getLocalAudioPluginService(context, serviceClassName) or findLocalPluginInformation().")
    fun getLocalAudioPluginService(context: Context) =
        AudioPluginHostHelper.selectPrimaryAudioPluginService(getLocalAudioPluginServices(context))
            ?: throw AudioPluginException("No AudioPluginService was found in ${context.packageName}.")

    /** Returns every AudioPluginService in this application package. */
    @JvmStatic
    fun getLocalAudioPluginServices(context: Context): List<PluginServiceInformation> =
        AudioPluginHostHelper.queryAudioPluginServices(context, context.packageName).toList()

    /** Returns the AudioPluginService in this application package whose class is [serviceClassName]. */
    @JvmStatic
    fun getLocalAudioPluginService(context: Context, serviceClassName: String): PluginServiceInformation {
        val serviceInfo = context.packageManager.getServiceInfo(
            ComponentName(context.packageName, serviceClassName), PackageManager.GET_META_DATA)
        return AudioPluginHostHelper.createAudioPluginServiceInformation(context, serviceInfo)
            ?: throw AudioPluginException("AudioPluginService '$serviceClassName' has no readable AAP metadata.")
    }

    /** Returns the plugin [pluginId] in this application package, whichever AudioPluginService hosts it. */
    @JvmStatic
    fun findLocalPluginInformation(context: Context, pluginId: String): PluginInformation? =
        getLocalAudioPluginServices(context).firstNotNullOfOrNull { svc ->
            svc.plugins.firstOrNull { it.pluginId == pluginId } }

    @JvmStatic
    fun getForegroundServiceType(context: Context, packageName: String, serviceClassName: String) =
        context.packageManager.getServiceInfo(ComponentName(packageName, serviceClassName), 0).foregroundServiceType

    @JvmStatic
    fun getServiceInstance(pluginId: String): Long {
        val instanceId = currentInstanceId.get()
        return if (instanceId != null)
            getServiceInstanceForInstance(pluginId, instanceId)
        else
            getServiceInstanceNative(pluginId)
    }

    @JvmStatic
    private external fun getServiceInstanceNative(pluginId: String): Long

    @JvmStatic
    private external fun getServiceInstanceForInstance(pluginId: String, instanceId: Int): Long

    // It is used by AudioPluginViewService (which makes use of SurfaceControlViewHost).
    @JvmStatic
    fun getNativeViewPreferredSize(context: Context, pluginId: String, instanceId: Int): Size? =
        withInstanceScope(instanceId) {
            createNativeViewFactory(context, pluginId).getPreferredSize(context, pluginId, instanceId)
        }

    @JvmStatic
    fun createNativeView(context: Context, pluginId: String, instanceId: Int): View =
        withInstanceScope(instanceId) {
            createNativeViewFactory(context, pluginId).createView(context, pluginId, instanceId)
        }

    private fun createNativeViewFactory(context: Context, pluginId: String): AudioPluginViewFactory {
        val pluginInfo = findLocalPluginInformation(context, pluginId)
            ?: throw AudioPluginException("Specified plugin '$pluginId' was not found")
        val factoryClassName = pluginInfo.uiViewFactory
            ?: throw AudioPluginException("'ui-view-factory' attribute is not specified in aap_metadata.xml")
        val cls = Class.forName(factoryClassName)
        if (!AudioPluginViewFactory::class.java.isAssignableFrom(cls))
            throw AudioPluginException("The class '$factoryClassName' specified by 'ui-view-factory' attribute in aap_metadata.xml must be derived from AudioPluginViewFactory.")
        val factory = cls.getConstructor().newInstance() as AudioPluginViewFactory
        return factory
    }

    private inline fun <T> withInstanceScope(instanceId: Int, block: () -> T): T {
        val previous = currentInstanceId.get()
        currentInstanceId.set(instanceId)
        return try {
            block()
        } finally {
            if (previous == null)
                currentInstanceId.remove()
            else
                currentInstanceId.set(previous)
        }
    }
}
