package org.androidaudioplugin

// The requested state of an audio bus for `applyBusLayout()`. An empty `layout` means the default.
data class BusLayoutRequest(val id: Int, val enabled: Boolean, val channelCount: Int, val layout: String = "")
