package org.androidaudioplugin;

oneway interface AudioPluginInterfaceCallback {
    void hostExtension(int instanceId, String uri, int opcode);
    void requestProcess(int instanceId);
}
