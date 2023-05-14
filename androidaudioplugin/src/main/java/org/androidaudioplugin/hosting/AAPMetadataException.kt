package org.androidaudioplugin.hosting

class AAPMetadataException : Exception {
    constructor() : super("Error in AAP metadata")
    constructor(message: String) : super(message)
    constructor(message: String, innerException: Exception) : super(message, innerException)
}