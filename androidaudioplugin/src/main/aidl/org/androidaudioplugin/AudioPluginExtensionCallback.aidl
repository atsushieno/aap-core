package org.androidaudioplugin;

oneway interface AudioPluginExtensionCallback {
    void completed(int instanceId, int requestId, String errorMessage);
}
