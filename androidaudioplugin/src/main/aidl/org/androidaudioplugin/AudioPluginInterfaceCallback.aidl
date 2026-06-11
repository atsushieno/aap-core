package org.androidaudioplugin;

import org.androidaudioplugin.AudioPluginExtensionCallback;

oneway interface AudioPluginInterfaceCallback {
    void hostExtension(int instanceId, String uri, int opcode, int requestId, in AudioPluginExtensionCallback callback);
    void requestProcess(int instanceId);
}
