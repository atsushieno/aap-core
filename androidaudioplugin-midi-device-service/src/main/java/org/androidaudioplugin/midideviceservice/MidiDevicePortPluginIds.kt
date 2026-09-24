package org.androidaudioplugin.midideviceservice

import android.content.ComponentName
import android.content.Context
import android.content.pm.PackageManager
import android.media.midi.MidiDeviceService
import android.media.midi.MidiUmpDeviceService
import android.util.Log
import androidx.annotation.RequiresApi
import org.androidaudioplugin.hosting.AudioPluginMidiDeviceMetadata

// Reads the `aap:plugin-id` of each port from the MIDI device XML resource of the service itself.
// See AudioPluginMidiDeviceMetadata for the format.
internal object MidiDevicePortPluginIds {
    private const val LOG_TAG = "AAP.MidiDeviceService"

    fun readForMidiDeviceService(service: Context): List<String?> = try {
        val serviceInfo = service.packageManager.getServiceInfo(
            ComponentName(service, service.javaClass), PackageManager.GET_META_DATA)
        serviceInfo.loadXmlMetaData(service.packageManager, MidiDeviceService.SERVICE_INTERFACE)?.use {
            AudioPluginMidiDeviceMetadata.readPortPluginIds(it, AudioPluginMidiDeviceMetadata.MIDI1_PORT_ELEMENT)
        } ?: listOf()
    } catch (ex: Exception) {
        Log.w(LOG_TAG, "Failed to read the MIDI device metadata of ${service.javaClass.name}", ex)
        listOf()
    }

    @RequiresApi(35)
    fun readForMidiUmpDeviceService(service: Context): List<String?> = try {
        val property = service.packageManager.getProperty(
            MidiUmpDeviceService.SERVICE_INTERFACE, ComponentName(service, service.javaClass))
        service.resources.getXml(property.resourceId).use {
            AudioPluginMidiDeviceMetadata.readPortPluginIds(it, AudioPluginMidiDeviceMetadata.UMP_PORT_ELEMENT)
        }
    } catch (ex: Exception) {
        Log.w(LOG_TAG, "Failed to read the UMP device metadata of ${service.javaClass.name}", ex)
        listOf()
    }
}
