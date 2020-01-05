package org.androidaudioplugin

class AudioPluginServiceInformation(var name: String, var packageName: String,
                                    var className: String)
{
    var extensions = mutableListOf<String>()
    var plugins = mutableListOf<PluginInformation>()
}

