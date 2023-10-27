package org.androidaudioplugin.hosting

import android.os.RemoteException
import android.util.Log
import org.androidaudioplugin.ParameterInformation
import org.androidaudioplugin.PortInformation
import java.nio.ByteBuffer


enum class InstanceState {
    UNPREPARED,
    INACTIVE,
    ACTIVE,
    DESTROYED,
    ERROR
}

/* maps to aap::RemotePluginInstance */
class NativeRemotePluginInstance(val instanceId: Int, // aap::RemotePluginInstance*
                                 val client: Long) {

    // error handling
    var state = InstanceState.UNPREPARED
    private var proxyError: Exception? = null

    private fun runCatchingRemoteException(func: () -> Unit) {
        runCatchingRemoteException(Unit) {
            func()
        }
    }
    private fun <T> runCatchingRemoteException(default: T, func: () -> T) : T {
        if (state == InstanceState.ERROR)
            return default
        return try {
            func()
        } catch (ex: RemoteException) {
            state = InstanceState.ERROR
            proxyError = ex
            Log.e("AAP", "AudioPluginInstance received RemoteException: ${ex.message ?: ""}\n{${ex.stackTraceToString()}")
            default
        }
    }

    fun prepare(frameCount: Int, defaultControlBytesPerBlock: Int) = runCatchingRemoteException {
        prepare(client, instanceId, frameCount, defaultControlBytesPerBlock)
        state = InstanceState.INACTIVE
    }
    fun activate() = runCatchingRemoteException {
        activate(client, instanceId)
        state = InstanceState.ACTIVE
    }
    fun process(frameCount: Int, timeoutInNanoseconds: Long) = runCatchingRemoteException {
        process(client, instanceId, frameCount, timeoutInNanoseconds)
    }
    fun deactivate() = runCatchingRemoteException {
        deactivate(client, instanceId)
        state = InstanceState.INACTIVE
    }
    fun destroy() = runCatchingRemoteException {
        destroy(client, instanceId)
        state = InstanceState.DESTROYED
    }

    // standard extensions
    // state
    fun getStateSize() = runCatchingRemoteException(0) {
        getStateSize(client, instanceId)
    }
    fun getState(data: ByteArray) = runCatchingRemoteException {
        getState(client, instanceId, data)
    }
    fun setState(data: ByteArray) = runCatchingRemoteException {
        setState(client, instanceId, data)
    }
    // presets
    fun getPresetCount() = runCatchingRemoteException(0) {
        getPresetCount(client, instanceId)
    }
    fun getCurrentPresetIndex() = runCatchingRemoteException(0) {
        getCurrentPresetIndex(client, instanceId)
    }
    fun setCurrentPresetIndex(index: Int) = runCatchingRemoteException {
        setCurrentPresetIndex(client, instanceId, index)
    }
    fun getPresetName(index: Int) = runCatchingRemoteException("") {
        getPresetName(client, instanceId, index)
    }
    // midi
    fun getMidiMappingPolicy(): Int = runCatchingRemoteException(0) {
        getMidiMappingPolicy(client, instanceId)
    }
    // gui
    fun createGui() = runCatchingRemoteException(0) {
        createGui(client, instanceId)
    }
    fun showGui(guiInstanceId: Int) = runCatchingRemoteException(0) {
        showGui(client, instanceId, guiInstanceId)
    }
    fun hideGui(guiInstanceId: Int) = runCatchingRemoteException(0) {
        hideGui(client, instanceId, guiInstanceId)
    }
    fun resizeGui(guiInstanceId: Int, width: Int, height: Int) = runCatchingRemoteException(0) {
        resizeGui(client, instanceId, guiInstanceId, width, height)
    }
    fun destroyGui(guiInstanceId: Int) = runCatchingRemoteException(0) {
        destroyGui(client, instanceId, guiInstanceId)
    }

    fun addEventUmpInput(data: ByteBuffer, length: Int) = runCatchingRemoteException {
        addEventUmpInput(client, instanceId, data, length)
    }

    // plugin instance (dynamic) information retrieval
    fun getParameterCount() = runCatchingRemoteException(0) {
        getParameterCount(client, instanceId)
    }
    fun getParameter(index: Int) = getParameter(client, instanceId, index)
    fun getPortCount() = runCatchingRemoteException(0) {
        getPortCount(client, instanceId)
    }
    fun getPort(index: Int) = getPort(client, instanceId, index)
    fun getPortBuffer(portIndex: Int, buffer: ByteBuffer, size: Int) = runCatchingRemoteException {
        getPortBuffer(client, instanceId, portIndex, buffer, size)
    }
    fun setPortBuffer(portIndex: Int, buffer: ByteBuffer, size: Int) = runCatchingRemoteException {
        setPortBuffer(client, instanceId, portIndex, buffer, size)
    }

    fun sendExtensionRequest(uri: String, opcode: Int, buffer: ByteBuffer, offset: Int, length: Int) = runCatchingRemoteException {
        sendExtensionRequest(client, instanceId, uri, opcode, buffer, offset, length)
    }

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

        external fun sendExtensionRequest(nativeClient: Long, instanceId: Int, uri: String, opcode: Int, buffer: ByteBuffer, offset: Int, length: Int)

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
