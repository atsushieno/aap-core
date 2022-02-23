package org.androidaudioplugin

/**
 * The AAP extension data structure. An AAP extension consists of the URI as the identifier and
 * a ByteArray as its data. It is passed as an argument to the plugin's `instantiate()` (function
 * pointer) in `AndroidAudioPluginFactory` in the plugin C API.
 */
class AudioPluginExtensionData(val uri: String, val data: ByteArray)
