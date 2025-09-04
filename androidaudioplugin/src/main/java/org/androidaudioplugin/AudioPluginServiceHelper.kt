package org.androidaudioplugin

import android.content.ComponentName
import android.content.Context
import android.view.View
import org.androidaudioplugin.hosting.AudioPluginHostHelper

// It is used only by AudioPluginService and some plugin extensions (such as AudioPluginLv2ServiceExtension) to process client (plugin host) requests.
object AudioPluginServiceHelper {
    fun getLocalAudioPluginService(context: Context) =
        AudioPluginHostHelper.queryAudioPluginServices(context)
            .first { svc -> svc.packageName == context.packageName }

    @JvmStatic
    fun getForegroundServiceType(context: Context, packageName: String, serviceClassName: String) =
        context.packageManager.getServiceInfo(ComponentName(packageName, serviceClassName), 0).foregroundServiceType

    @JvmStatic
    external fun getServiceInstance(pluginId: String): Long

    // It is used by AudioPluginViewService (which makes use of SurfaceControlViewHost).
    @JvmStatic
    fun createNativeView(context: Context, pluginId: String, instanceId: Int): View {
        val pluginInfo = getLocalAudioPluginService(context).plugins.firstOrNull { it.pluginId == pluginId }
            ?: throw AudioPluginException("Specified plugin '$pluginId' was not found")
        val factoryClassName = pluginInfo.uiViewFactory
            ?: throw AudioPluginException("'ui-view-factory' attribute is not specified in aap_metadata.xml")
        val cls = Class.forName(factoryClassName)
        if (!AudioPluginViewFactory::class.java.isAssignableFrom(cls))
            throw AudioPluginException("The class '$factoryClassName' specified by 'ui-view-factory' attribute in aap_metadata.xml must be derived from AudioPluginViewFactory.")
        val factory = cls.getConstructor().newInstance() as AudioPluginViewFactory
        return factory.createView(context, pluginId, instanceId)
    }
}
