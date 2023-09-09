package org.androidaudioplugin

import java.nio.ByteBuffer

class NativeLocalPluginInstance(private val service: NativePluginService, private val instanceId: Int) {

    fun getPluginId() = getPluginId(service.native, instanceId)
    fun getPortCount() = getPortCount(service.native, instanceId)
    fun getPort(index: Int) = getPort(service.native, instanceId, index)
    fun getParameterCount() = getParameterCount(service.native, instanceId)
    fun getParameter(index: Int) = getParameter(service.native, instanceId, index)

    fun addEventUmpInput(data: ByteBuffer, size: Int) = addEventUmpInput(service.native, instanceId, data, size)

    // FIXME: we should probably generalize extension features, instead of defining functions and properties for all.
    fun getPresetCount() = getPresetCount(service.native, instanceId)

    fun getPresetName(index: Int) = getPresetName(service.native, instanceId, index)
    fun setPresetIndex(index: Int) = setPresetIndex(service.native, instanceId, index)

    companion object {
        @JvmStatic
        private external fun getPluginId(nativeService: Long, instanceId: Int): String

        @JvmStatic
        private external fun getPortCount(nativeService: Long, instanceId: Int): Int
        @JvmStatic
        private external fun getPort(nativeService: Long, instanceId: Int, index: Int): PortInformation
        @JvmStatic
        private external fun getParameterCount(nativeService: Long, instanceId: Int): Int
        @JvmStatic
        private external fun getParameter(nativeService: Long, instanceId: Int, index: Int): ParameterInformation

        @JvmStatic
        private external fun addEventUmpInput(nativeService: Long, instanceId: Int, data: ByteBuffer, size: Int)

        // FIXME: we should probably generalize extension features, instead of defining functions and properties for all.
        @JvmStatic
        private external fun getPresetCount(nativeService: Long, instanceId: Int) : Int
        @JvmStatic
        private external fun getPresetName(nativeService: Long, instanceId: Int, index: Int) : String
        @JvmStatic
        private external fun setPresetIndex(nativeService: Long, instanceId: Int, index: Int)
    }
}