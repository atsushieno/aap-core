package org.androidaudioplugin

class AudioPluginException : Exception {
    constructor() : this ("AudioPlugin error")
    constructor(message: String) : super(message)
    constructor(message: String, innerException: Exception) : super(message, innerException)
}