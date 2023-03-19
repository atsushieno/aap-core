package org.androidaudioplugin;

oneway interface AudioPluginInterfaceCallback {
    void hostExtension(int instanceId, String uri, int size);
    void requestProcess(int instanceId);
}
