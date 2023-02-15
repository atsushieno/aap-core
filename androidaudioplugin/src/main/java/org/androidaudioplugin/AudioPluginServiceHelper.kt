package org.androidaudioplugin

import android.content.Context
import org.androidaudioplugin.hosting.AudioPluginHostHelper

// It is used only by AudioPluginService and some plugin extensions (such as AudioPluginLv2ServiceExtension) to process client (plugin host) requests.
object AudioPluginServiceHelper {
    fun getLocalAudioPluginService(context: Context) =
        AudioPluginHostHelper.queryAudioPluginServices(context)
            .first { svc -> svc.packageName == context.packageName }

    @JvmStatic
    fun createGui(context: Context, pluginId: String, instanceId: Int) : AudioPluginView {
        val pluginInfo = getLocalAudioPluginService(context).plugins.firstOrNull { it.pluginId == pluginId }
            ?: throw AudioPluginException("SpecifiedPlugin was not found")
        val factoryClassName = pluginInfo.uiViewFactory
            ?: throw AudioPluginException("'ui-view-factory' attribute is not specified in aap_metadata.xml")
        val cls = Class.forName(factoryClassName)
        if (!AudioPluginViewFactory::class.java.isAssignableFrom(cls))
            throw AudioPluginException("The class specified by 'ui-view-factory' attribute in aap_metadata.xml must be derived from AudioPluginViewFactory.")
        val factory = cls.newInstance() as AudioPluginViewFactory
        val view = factory.createView(context, pluginId, instanceId)
        val audioPluginView = AudioPluginView(context)
        audioPluginView.addView(view)
        return audioPluginView
    }

    @JvmStatic
    external fun getServiceInstance(pluginId: String): Long
}
