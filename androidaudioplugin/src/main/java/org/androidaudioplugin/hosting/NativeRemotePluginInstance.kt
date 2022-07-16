package org.androidaudioplugin.hosting

import android.os.ParcelFileDescriptor
import org.androidaudioplugin.PortInformation
import kotlin.properties.Delegates


/* maps to aap::RemotePluginInstance */
class NativeRemotePluginInstance(val pluginId: String,
                                 sampleRate: Int,
                                 val client: NativePluginClient) {
    fun beginPrepare(frameCount: Int, portCount: Int) {
        nativeAudioPluginBuffer = createAudioPluginBuffer(frameCount, portCount)
        beginPrepare(client.native, instanceId, nativeAudioPluginBuffer)
    }
    fun prepareMemory(index: Int, fd: ParcelFileDescriptor) {
        setSharedMemoryToPluginBuffer(nativeAudioPluginBuffer, index, fd.fd)
    }
    fun endPrepare() = endPrepare(client.native, instanceId, nativeAudioPluginBuffer)
    fun activate() = activate(client.native, instanceId)
    fun process(timeoutInNanoseconds: Int) = process(client.native, instanceId, nativeAudioPluginBuffer, timeoutInNanoseconds)
    fun deactivate() = deactivate(client.native, instanceId)
    fun destroy() {
        destroyAudioPluginBuffer(nativeAudioPluginBuffer)
        destroy(client.native, instanceId)
    }

    fun getStateSize() : Int = getStateSize(client.native, instanceId)
    fun getState(data: ByteArray) = getState(client.native, instanceId, data)
    fun setState(data: ByteArray) = setState(client.native, instanceId, data)

    fun getPresetCount() = getPresetCount(client.native, instanceId)
    fun getCurrentPresetIndex() = getCurrentPresetIndex(client.native, instanceId)
    fun setCurrentPresetIndex(index: Int) = setCurrentPresetIndex(client.native, instanceId, index)
    fun getCurrentPresetName(index: Int) = getCurrentPresetName(client.native, instanceId, index)

    // aap::RemotePluginInstance*
    private val instanceId: Int = createRemotePluginInstance(pluginId, sampleRate, client.native)
    private var nativeAudioPluginBuffer by Delegates.notNull<Long>()

    fun getPortCount() = getPortCount(client.native, instanceId)
    fun getPort(index: Int) = getPort(client.native, instanceId, index)

    companion object {
        @JvmStatic
        private external fun createAudioPluginBuffer(frameCount: Int, portCount: Int) : Long
        @JvmStatic
        private external fun destroyAudioPluginBuffer(pointer: Long)
        // Note that it returns an instanceId within the client, not the pointer to the instance.
        // Therefore it returns Int, not Long.
        @JvmStatic
        private external fun createRemotePluginInstance(pluginId: String, sampleRate: Int, nativeClient: Long) : Int
        @JvmStatic
        external fun setSharedMemoryToPluginBuffer(nativeAudioPluginBuffer: Long, index: Int, fd: Int)
        @JvmStatic
        external fun beginPrepare(nativeClient: Long, instanceId: Int, nativeAudioPluginBuffer: Long)
        @JvmStatic
        external fun endPrepare(nativeClient: Long, instanceId: Int, nativeAudioPluginBuffer: Long)
        @JvmStatic
        external fun activate(nativeClient: Long, instanceId: Int)
        @JvmStatic
        external fun process(nativeClient: Long, instanceId: Int, nativeAudioPluginBuffer: Long, timeoutInNanoseconds: Int)
        @JvmStatic
        external fun deactivate(nativeClient: Long, instanceId: Int)
        @JvmStatic
        external fun destroy(nativeClient: Long, instanceId: Int)

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
        external fun getCurrentPresetName(nativeClient: Long, instanceId: Int, index: Int) : String

        @JvmStatic
        external fun getPortCount(nativeClient: Long, instanceId: Int) : Int

        @JvmStatic
        external fun getPort(nativeClient: Long, instanceId: Int, index: Int) : PortInformation
    }
}
