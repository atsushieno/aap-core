package org.androidaudioplugin

import android.content.Context
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import kotlin.coroutines.coroutineContext

// It is used only by AudioPluginService and some plugin extensions (such as AudioPluginLv2ServiceExtension) to process client (plugin host) requests.
object AudioPluginServiceHelper {
    fun getLocalAudioPluginService(context: Context) =
        AudioPluginHostHelper.queryAudioPluginServices(context)
            .first { svc -> svc.packageName == context.packageName }

    @JvmStatic
    external fun getServiceInstance(pluginId: String): Long

    @OptIn(DelicateCoroutinesApi::class)
    @JvmStatic
    fun createGui(context: Context, pluginId: String, instanceId: Int) : AudioPluginView {
        val pluginInfo = getLocalAudioPluginService(context).plugins.firstOrNull { it.pluginId == pluginId }
            ?: throw AudioPluginException("Specified plugin '$pluginId' was not found")
        val factoryClassName = pluginInfo.uiViewFactory
            ?: throw AudioPluginException("'ui-view-factory' attribute is not specified in aap_metadata.xml")
        val cls = Class.forName(factoryClassName)
        val audioPluginView = AudioPluginView(context)
        // content is asynchronously filled
        Dispatchers.Main.dispatch(GlobalScope.coroutineContext) {
            if (!AudioPluginViewFactory::class.java.isAssignableFrom(cls))
                throw AudioPluginException("The class '$factoryClassName' specified by 'ui-view-factory' attribute in aap_metadata.xml must be derived from AudioPluginViewFactory.")
            val factory = cls.newInstance() as AudioPluginViewFactory
            val view = factory.createView(context, pluginId, instanceId)
            audioPluginView.addView(view)
        }
        return audioPluginView
    }

    @OptIn(DelicateCoroutinesApi::class)
    @JvmStatic
    fun showGui(context: Context, pluginId: String, instanceId: Int, view: AudioPluginView) {
        Dispatchers.Main.dispatch(GlobalScope.coroutineContext) {
            view.showOverlay()
        }
    }

    @OptIn(DelicateCoroutinesApi::class)
    @JvmStatic
    fun hideGui(context: Context, pluginId: String, instanceId: Int, view: AudioPluginView) {
        Dispatchers.Main.dispatch(GlobalScope.coroutineContext) {
            view.hideOverlay(false)
        }
    }

    @JvmStatic
    fun destroyGui(context: Context, pluginId: String, instanceId: Int, view: AudioPluginView) {
    }
}
