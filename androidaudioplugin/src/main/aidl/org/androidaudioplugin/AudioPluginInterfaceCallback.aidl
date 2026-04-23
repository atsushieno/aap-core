package org.androidaudioplugin;

oneway interface AudioPluginInterfaceCallback {
    void extensionReply(int instanceId, String uri, int opcode, int requestId);
    void hostExtension(int instanceId, String uri, int opcode);
    void requestProcess(int instanceId);
}
