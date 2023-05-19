package org.androidaudioplugin

import android.content.Context
import android.view.View
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import org.androidaudioplugin.hosting.AudioPluginHostHelper

// It is used only by AudioPluginService and some plugin extensions (such as AudioPluginLv2ServiceExtension) to process client (plugin host) requests.
object AudioPluginServiceHelper {
    fun getLocalAudioPluginService(context: Context) =
        AudioPluginHostHelper.queryAudioPluginServices(context)
            .first { svc -> svc.packageName == context.packageName }

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
        val factory = cls.newInstance() as AudioPluginViewFactory
        return factory.createView(context, pluginId, instanceId)
    }

    @OptIn(DelicateCoroutinesApi::class)
    @JvmStatic
    fun createSystemAlertView(context: Context, pluginId: String, instanceId: Int) : AudioPluginSystemAlertView {
        val audioPluginView = AudioPluginSystemAlertView(context)
        // content is asynchronously filled
        Dispatchers.Main.dispatch(GlobalScope.coroutineContext) {
            val view = createNativeView(context, pluginId, instanceId)
            audioPluginView.addView(view)
        }
        return audioPluginView
    }

    @OptIn(DelicateCoroutinesApi::class)
    @JvmStatic
    fun showGui(context: Context, pluginId: String, instanceId: Int, view: AudioPluginSystemAlertView) {
        Dispatchers.Main.dispatch(GlobalScope.coroutineContext) {
            view.showOverlay()
        }
    }

    @OptIn(DelicateCoroutinesApi::class)
    @JvmStatic
    fun hideGui(context: Context, pluginId: String, instanceId: Int, view: AudioPluginSystemAlertView) {
        Dispatchers.Main.dispatch(GlobalScope.coroutineContext) {
            view.hideOverlay(false)
        }
    }

    @JvmStatic
    fun destroyGui(context: Context, pluginId: String, instanceId: Int, view: AudioPluginSystemAlertView) {
    }
}
