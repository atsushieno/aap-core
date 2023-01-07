package org.androidaudioplugin.hosting

import org.androidaudioplugin.ParameterInformation
import org.androidaudioplugin.PortInformation
import java.nio.ByteBuffer


/* maps to aap::RemotePluginInstance */
internal class NativeRemotePluginInstance(val pluginId: String,
                                 sampleRate: Int,
                                 val client: NativePluginClient) {
    fun prepare(frameCount: Int, defaultControlBytesPerBlock: Int) = prepare(client.native, instanceId, frameCount, defaultControlBytesPerBlock)
    fun activate() = activate(client.native, instanceId)
    fun process(timeoutInNanoseconds: Int) = process(client.native, instanceId, timeoutInNanoseconds)
    fun deactivate() = deactivate(client.native, instanceId)
    fun destroy() {
        destroy(client.native, instanceId)
    }

    fun getStateSize() : Int = getStateSize(client.native, instanceId)
    fun getState(data: ByteArray) = getState(client.native, instanceId, data)
    fun setState(data: ByteArray) = setState(client.native, instanceId, data)

    fun getPresetCount() = getPresetCount(client.native, instanceId)
    fun getCurrentPresetIndex() = getCurrentPresetIndex(client.native, instanceId)
    fun setCurrentPresetIndex(index: Int) = setCurrentPresetIndex(client.native, instanceId, index)
    fun getPresetName(index: Int) = getPresetName(client.native, instanceId, index)

    // aap::RemotePluginInstance*
    val instanceId: Int = createRemotePluginInstance(pluginId, sampleRate, client.native)

    fun getParameterCount() = getParameterCount(client.native, instanceId)
    fun getParameter(index: Int) = getParameter(client.native, instanceId, index)
    fun getPortCount() = getPortCount(client.native, instanceId)
    fun getPort(index: Int) = getPort(client.native, instanceId, index)
    fun getPortBuffer(portIndex: Int, buffer: ByteBuffer, size: Int) = getPortBuffer(client.native, instanceId, portIndex, buffer, size)
    fun setPortBuffer(portIndex: Int, buffer: ByteBuffer, size: Int) = setPortBuffer(client.native, instanceId, portIndex, buffer, size)
    fun getMidiMappingPolicy(): Int = getMidiMappingPolicy(client.native, instanceId)

    companion object {
        // Note that it returns an instanceId within the client, not the pointer to the instance.
        // Therefore it returns Int, not Long.
        @JvmStatic
        private external fun createRemotePluginInstance(pluginId: String, sampleRate: Int, nativeClient: Long) : Int
        @JvmStatic
        external fun prepare(nativeClient: Long, instanceId: Int, frameCount: Int, defaultControlBytesPerBlock: Int)
        @JvmStatic
        external fun activate(nativeClient: Long, instanceId: Int)
        @JvmStatic
        external fun process(nativeClient: Long, instanceId: Int, timeoutInNanoseconds: Int)
        @JvmStatic
        external fun deactivate(nativeClient: Long, instanceId: Int)
        @JvmStatic
        external fun destroy(nativeClient: Long, instanceId: Int)

        @JvmStatic
        external fun getParameterCount(nativeClient: Long, instanceId: Int) : Int

        @JvmStatic
        external fun getParameter(nativeClient: Long, instanceId: Int, index: Int) : ParameterInformation

        @JvmStatic
        external fun getPortCount(nativeClient: Long, instanceId: Int) : Int

        @JvmStatic
        external fun getPort(nativeClient: Long, instanceId: Int, index: Int) : PortInformation
        @JvmStatic
        external fun getPortBuffer(nativeClient: Long, instanceId: Int, portIndex: Int, buffer: ByteBuffer, size: Int)

        @JvmStatic
        external fun setPortBuffer(nativeClient: Long, instanceId: Int, portIndex: Int, buffer: ByteBuffer, size: Int)

        @JvmStatic
        external fun getMidiMappingPolicy(nativeClient: Long, instanceId: Int) : Int

        // Standard Extensions

        // state
        @JvmStatic
        external fun getStateSize(nativeClient: Long, instanceId: Int) : Int
        @JvmStatic
        external fun getState(nativeClient: Long, instanceId: Int, data: ByteArray)
        @JvmStatic
        external fun setState(nativeClient: Long, instanceId: Int, data: ByteArray)

        // presets
        @JvmStatic
        external fun getPresetCount(nativeClient: Long, instanceId: Int) : Int
        @JvmStatic
        external fun  getCurrentPresetIndex(nativeClient: Long, instanceId: Int) : Int
        @JvmStatic
        external fun setCurrentPresetIndex(nativeClient: Long, instanceId: Int, index: Int)
        @JvmStatic
        external fun getPresetName(nativeClient: Long, instanceId: Int, index: Int) : String
    }
}
