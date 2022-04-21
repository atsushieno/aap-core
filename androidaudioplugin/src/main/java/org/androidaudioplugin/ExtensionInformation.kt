package org.androidaudioplugin

// We are sloppy here, instead of causing crash at loading invalid plugin info...
class ExtensionInformation(var required: Boolean, var uri: String?)
