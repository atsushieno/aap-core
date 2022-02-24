package org.androidaudioplugin

import android.content.Context
import android.os.IBinder
import android.os.SharedMemory

internal class AudioPluginNatives
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
        external fun initializeLocalHostForPluginService(pluginInfos: Array<PluginInformation>? = null)

        @JvmStatic
        external fun cleanupLocalHostNatives()

        @JvmStatic
        external fun createBinderForService(sampleRate: Int) : IBinder

        @JvmStatic
        external fun destroyBinderForService(binder: IBinder)

        @JvmStatic
        external fun addBinderForHost(packageName: String, className: String, binder: IBinder)

        @JvmStatic
        external fun removeBinderForHost(packageName: String, className: String)

        @JvmStatic
        external fun getSharedMemoryFD(shm: SharedMemory) : Int

        @JvmStatic
        external fun closeSharedMemoryFD(fd: Int)
    }
}