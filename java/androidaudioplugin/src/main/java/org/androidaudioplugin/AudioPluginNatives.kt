package org.androidaudioplugin

import android.content.Context
import android.os.IBinder
import android.os.SharedMemory

class AudioPluginNatives
{
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
        }

        @JvmStatic
        external fun setApplicationContext(applicationContext: Context)

        @JvmStatic
        external fun initialize(pluginInfos: Array<PluginInformation>)

        @JvmStatic
        external fun initializeLocalHost(pluginInfos: Array<PluginInformation>)

        @JvmStatic
        external fun cleanupLocalHostNatives()

        @JvmStatic
        external fun createBinderForService(sampleRate: Int) : IBinder

        @JvmStatic
        external fun destroyBinderForService(binder: IBinder)

        @JvmStatic
        external fun addBinderForHost(serviceIdentifier: String, binder: IBinder)

        @JvmStatic
        external fun removeBinderForHost(serviceIdentifier: String)

        @JvmStatic
        external fun getSharedMemoryFD(shm: SharedMemory) : Int

        @JvmStatic
        external fun closeSharedMemoryFD(fd: Int)
    }
}