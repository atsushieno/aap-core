package org.androidaudioplugin;

oneway interface AudioPluginInterfaceCallback {
    void notify(int instanceId, String uri, int size);
}
