package org.androidaudioplugin

import java.nio.ByteBuffer

class NativeLocalPluginInstance(private val service: NativePluginService, private val instanceId: Int) {

    fun getPortCount() = getPortCount(service.native, instanceId)
    fun getPort(index: Int) = getPort(service.native, instanceId, index)
    fun getParameterCount() = getParameterCount(service.native, instanceId)
    fun getParameter(index: Int) = getParameter(service.native, instanceId, index)

    companion object {
        @JvmStatic
        external fun getPortCount(nativeService: Long, instanceId: Int): Int
        @JvmStatic
        external fun getPort(nativeService: Long, instanceId: Int, index: Int): PortInformation
        @JvmStatic
        external fun getParameterCount(nativeService: Long, instanceId: Int): Int
        @JvmStatic
        external fun getParameter(nativeService: Long, instanceId: Int, index: Int): ParameterInformation
    }
}