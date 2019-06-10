package org.androidaudiopluginframework.hosting

class AAPLV2Host
{
    companion object {
        init {
            System.loadLibrary("aaphost")
        }

        @JvmStatic
        external fun runHost(pluginUris: Array<String>) : Int
        @JvmStatic
        external fun runHostOne(pluginUri: String) : Int
    }
}