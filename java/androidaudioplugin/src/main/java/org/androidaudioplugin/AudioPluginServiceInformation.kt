package org.androidaudioplugin

class AudioPluginServiceInformation
{
    var label: String
    var packageName: String
    var className: String
    var extensions = mutableListOf<String>()
    var plugins = mutableListOf<PluginInformation>()

    constructor(label: String, packageName: String, className: String)
    {
        this.label = label
        this.packageName = packageName
        this.className = className
    }
}

