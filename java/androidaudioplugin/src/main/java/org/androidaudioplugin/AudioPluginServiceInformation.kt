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

    constructor(label: String, serviceIdentifier: String)
    {
        this.label = label
        var idx = serviceIdentifier.indexOf('/')
        packageName = serviceIdentifier.substring(0, idx)
        className = serviceIdentifier.substring(idx + 1)
    }
}

