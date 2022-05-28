package org.androidaudioplugin;

oneway interface AudioPluginInterfaceCallback {
    void notify(int instanceId, int portId, int size);
}
