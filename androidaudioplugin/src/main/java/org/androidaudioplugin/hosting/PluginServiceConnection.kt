package org.androidaudioplugin.hosting

import android.content.ServiceConnection
import android.os.IBinder
import org.androidaudioplugin.AudioPluginExtensionData
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceInformation

class PluginServiceConnection(val platformServiceConnection: ServiceConnection,  val serviceInfo: PluginServiceInformation, val binder: IBinder, val instances: MutableList<AudioPluginInstance> = mutableListOf())
