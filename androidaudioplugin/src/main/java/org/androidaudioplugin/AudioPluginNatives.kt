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
        external fun initializeAAPJni(applicationContext: Context)

        @JvmStatic
        external fun createBinderForService() : IBinder

        @JvmStatic
        external fun destroyBinderForService(binder: IBinder)

        @JvmStatic
        external fun prepareNativeLooper()

        @JvmStatic
        external fun startNativeLooper()

        @JvmStatic
        external fun stopNativeLooper()

        @JvmStatic
        external fun addBinderForClient(connectorInstanceId: Int, packageName: String, className: String, binder: IBinder)

        @JvmStatic
        external fun removeBinderForClient(connectorInstanceId: Int, packageName: String, className: String)

        @JvmStatic
        external fun getSharedMemoryFD(shm: SharedMemory) : Int

        @JvmStatic
        external fun closeSharedMemoryFD(fd: Int)
    }
}