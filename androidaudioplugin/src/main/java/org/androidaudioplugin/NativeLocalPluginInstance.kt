package org.androidaudioplugin

import java.nio.ByteBuffer

class NativeLocalPluginInstance(private val service: NativePluginService, private val instanceId: Int) {

    fun getPluginId() = getPluginId(service.native, instanceId)
    fun getPortCount() = getPortCount(service.native, instanceId)
    fun getPort(index: Int) = getPort(service.native, instanceId, index)
    fun getParameterCount() = getParameterCount(service.native, instanceId)
    fun getParameter(index: Int) = getParameter(service.native, instanceId, index)

    fun addEventUmpInput(data: ByteBuffer, size: Int) = addEventUmpInput(service.native, instanceId, data, size)

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
    }
}