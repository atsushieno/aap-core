package org.androidaudioplugin.hosting

import android.content.Context
import android.os.ParcelFileDescriptor
import org.androidaudioplugin.AudioPluginNatives
import org.androidaudioplugin.hosting.AudioPluginClientBase
import org.androidaudioplugin.hosting.PluginServiceConnection
import kotlin.properties.Delegates

/*

1) In AudioPluginInstance.kt, replace `proxy = AudioPluginInterface.Stub.asInterface(service.binder!!)` with
   native RemotePluginInstance creation.


 */


class PluginClient(applicationContext: Context, val connection: PluginServiceConnection) : AudioPluginClientBase(applicationContext) {

    // aap::RemotePluginInstance*
    val native: Long = newInstance()

    fun createInstanceFromExistingConnection(pluginId: String) : RemotePluginInstance {
        return RemotePluginInstance(pluginId, this)
    }

    private fun newInstance() : Long {
        return newInstance(serviceConnector.instanceId)
    }

    companion object {
        @JvmStatic
        private external fun newInstance(connectorInstanceId: Int) : Long
    }
}

interface AudioPluginInterface2 {
    fun prepareMemory(instanceId: Int, index: Int, fd: ParcelFileDescriptor)
    fun prepare(instanceId: Int, frameCount: Int, portCount: Int)
    fun activate(instanceId: Int)
    fun process(instanceId: Int, timeoutInNanoseconds: Int)
    fun deactivate(instanceId: Int)
    fun destroy(instanceId: Int)

    // Standard Extensions
    fun getStateSize(instanceId: Int) : Int
    fun getState(instanceId: Int, data: ByteArray)
    fun setState(instanceId: Int, data: ByteArray)
}

class ExtensionWrapper(native: Long)

/* maps to aap::RemotePluginInstance */
class RemotePluginInstance(val pluginId: String,
                           val client: PluginClient) : AudioPluginInterface2 {
    override fun prepareMemory(instanceId: Int, index: Int, fd: ParcelFileDescriptor) {

    }
    override fun prepare(instanceId: Int, frameCount: Int, portCount: Int) {
        nativeAudioPluginBuffer = createAudioPluginBuffer(frameCount, portCount)
        prepare(native, nativeAudioPluginBuffer)
    }
    override fun activate(instanceId: Int) = activate(native)
    override fun process(instanceId: Int, timeoutInNanoseconds: Int) = process(native, nativeAudioPluginBuffer, timeoutInNanoseconds)
    override fun deactivate(instanceId: Int) = deactivate(native)
    override fun destroy(instanceId: Int) = destroy(native)

    override fun getStateSize(instanceId: Int) : Int = getStateSize(native)
    override fun getState(instanceId: Int, data: ByteArray) = getState(native, data)
    override fun setState(instanceId: Int, data: ByteArray) = setState(native, data)

    // aap::RemotePluginInstance*
    private val native: Long = createRemotePluginInstance(pluginId, client.sampleRate, client.native)
    private var nativeAudioPluginBuffer by Delegates.notNull<Long>()

    companion object {
        @JvmStatic
        private external fun createAudioPluginBuffer(frameCount: Int, portCount: Int) : Long
        @JvmStatic
        private external fun destroyAudioPluginBuffer(pointer: Long)
        @JvmStatic
        private external fun createRemotePluginInstance(pluginId: String, sampleRate: Int, client: Long) : Long
        @JvmStatic
        external fun prepare(native: Long, nativeAudioPluginBuffer: Long)
        @JvmStatic
        external fun activate(native: Long)
        @JvmStatic
        external fun process(native: Long, nativeAudioPluginBuffer: Long, timeoutInNanoseconds: Int)
        @JvmStatic
        external fun deactivate(native: Long)
        @JvmStatic
        external fun destroy(native: Long)
        @JvmStatic
        external fun getStateSize(native: Long) : Int
        @JvmStatic
        external fun getState(native: Long, data: ByteArray)
        @JvmStatic
        external fun setState(native: Long, data: ByteArray)
    }
}
