package org.androidaudioplugin.hosting

import org.androidaudioplugin.ParameterInformation
import org.androidaudioplugin.PortInformation
import java.nio.ByteBuffer


/* maps to aap::RemotePluginInstance */
class NativeRemotePluginInstance(val instanceId: Int, // aap::RemotePluginInstance*
                                 private val client: Long) {

    fun prepare(frameCount: Int, defaultControlBytesPerBlock: Int) = prepare(client, instanceId, frameCount, defaultControlBytesPerBlock)
    fun activate() = activate(client, instanceId)
    fun process(frameCount: Int, timeoutInNanoseconds: Long) = process(client, instanceId, frameCount, timeoutInNanoseconds)
    fun deactivate() = deactivate(client, instanceId)
    fun destroy() {
        destroy(client, instanceId)
    }

    // standard extensions
    // state
    fun getStateSize() : Int = getStateSize(client, instanceId)
    fun getState(data: ByteArray) = getState(client, instanceId, data)
    fun setState(data: ByteArray) = setState(client, instanceId, data)
    // presets
    fun getPresetCount() = getPresetCount(client, instanceId)
    fun getCurrentPresetIndex() = getCurrentPresetIndex(client, instanceId)
    fun setCurrentPresetIndex(index: Int) = setCurrentPresetIndex(client, instanceId, index)
    fun getPresetName(index: Int) = getPresetName(client, instanceId, index)
    // midi
    fun getMidiMappingPolicy(): Int = getMidiMappingPolicy(client, instanceId)
    // gui
    fun createGui() = createGui(client, instanceId)
    fun showGui(guiInstanceId: Int) = showGui(client, instanceId, guiInstanceId)
    fun hideGui(guiInstanceId: Int) = hideGui(client, instanceId, guiInstanceId)
    fun resizeGui(guiInstanceId: Int, width: Int, height: Int) = resizeGui(client, instanceId, guiInstanceId, width, height)
    fun destroyGui(guiInstanceId: Int) = destroyGui(client, instanceId, guiInstanceId)

    fun addEventUmpInput(data: ByteBuffer, length: Int) = addEventUmpInput(client, instanceId, data, length)

    // plugin instance (dynamic) information retrieval
    fun getParameterCount() = getParameterCount(client, instanceId)
    fun getParameter(index: Int) = getParameter(client, instanceId, index)
    fun getPortCount() = getPortCount(client, instanceId)
    fun getPort(index: Int) = getPort(client, instanceId, index)
    fun getPortBuffer(portIndex: Int, buffer: ByteBuffer, size: Int) = getPortBuffer(client, instanceId, portIndex, buffer, size)
    fun setPortBuffer(portIndex: Int, buffer: ByteBuffer, size: Int) = setPortBuffer(client, instanceId, portIndex, buffer, size)

    companion object {
        fun create(pluginId: String, sampleRate: Int, nativeClient: Long) =
            NativeRemotePluginInstance(createRemotePluginInstance(pluginId, sampleRate, nativeClient), nativeClient)

        // invoked from AAPJniFacade
        @JvmStatic
        fun fromNative(instanceId: Int, nativeClient: Long) = NativeRemotePluginInstance(instanceId, nativeClient)

        // Note that it returns an instanceId within the client, not the pointer to the instance.
        // Therefore it returns Int, not Long.
        @JvmStatic
        private external fun createRemotePluginInstance(pluginId: String, sampleRate: Int, nativeClient: Long) : Int
        @JvmStatic
        external fun prepare(nativeClient: Long, instanceId: Int, frameCount: Int, defaultControlBytesPerBlock: Int)
        @JvmStatic
        external fun activate(nativeClient: Long, instanceId: Int)
        @JvmStatic
        external fun process(nativeClient: Long, instanceId: Int, frameCount: Int, timeoutInNanoseconds: Long)
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

        // midi
        @JvmStatic
        external fun getMidiMappingPolicy(nativeClient: Long, instanceId: Int) : Int

        // gui
        @JvmStatic
        external fun createGui(nativeClient: Long, instanceId: Int) : Int
        @JvmStatic
        external fun showGui(nativeClient: Long, instanceId: Int, guiInstanceId: Int) : Int
        @JvmStatic
        external fun hideGui(nativeClient: Long, instanceId: Int, guiInstanceId: Int) : Int
        @JvmStatic
        external fun resizeGui(nativeClient: Long, instanceId: Int, guiInstanceId: Int, width: Int, height: Int) : Int
        @JvmStatic
        external fun destroyGui(nativeClient: Long, instanceId: Int, guiInstanceId: Int) : Int

        @JvmStatic
        external fun addEventUmpInput(nativeClient: Long, instanceId: Int, data: ByteBuffer, length: Int)
    }
}
