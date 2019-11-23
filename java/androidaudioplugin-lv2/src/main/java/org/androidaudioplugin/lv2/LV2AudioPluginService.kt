package org.androidaudioplugin.lv2
import org.androidaudioplugin.AudioPluginService

open class LV2AudioPluginService : AudioPluginService()
{
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
            System.loadLibrary("androidaudioplugin-lv2")
        }
    }
}
