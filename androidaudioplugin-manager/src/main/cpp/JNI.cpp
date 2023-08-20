#include <jni.h>

#include "PluginPlayer.h"

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_manager_PluginPlayer_loadAudioResourceNative(JNIEnv *env, jobject thiz,
                                                                         jlong player,
                                                                         jbyteArray bytes,
                                                                         jstring filename) {
    jboolean isDataCopy{false};
    auto data = (uint8_t*) env->GetByteArrayElements(bytes, &isDataCopy);
    jboolean isNameCopy{false};
    auto name = env->GetStringUTFChars(filename, &isNameCopy);
    ((aap::PluginPlayer*) player)->setAudioSource(data, env->GetArrayLength(bytes), name);
    if (isNameCopy)
        free((void*) name);
    if (isDataCopy)
        free((void*) data);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_manager_PluginPlayer_addMidiEventNative(JNIEnv *env, jobject thiz,
                                                                    jlong player, jbyteArray bytes,
                                                                    jint offset, jint length) {
    // TODO: implement addMidiEventNative()
    jboolean isDataCopy{false};
    auto data = (uint8_t*) env->GetByteArrayElements(bytes, &isDataCopy);
    ((aap::PluginPlayer*) player)->graph.addMidiEvent(data + offset, length);
    if (isDataCopy)
        free((void*) data);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_org_androidaudioplugin_manager_PluginPlayer_createNewPluginPlayer(JNIEnv *env, jclass clazz,
                                                                       jint sampleRate,
                                                                       jint framesPerCallback) {
    aap::PluginPlayerConfiguration configuration{sampleRate, framesPerCallback};
    return (jlong) (void*) new aap::PluginPlayer(configuration);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_manager_PluginPlayer_deletePluginPlayer(JNIEnv *env, jobject thiz,
                                                                    jlong native) {
    delete (aap::PluginPlayer*) native;
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_manager_PluginPlayer_startProcessingNative(JNIEnv *env, jobject thiz,
                                                                       jlong player) {
    ((aap::PluginPlayer*) player)->startProcessing();
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_manager_PluginPlayer_pauseProcessingNative(JNIEnv *env, jobject thiz,
                                                                       jlong player) {
    ((aap::PluginPlayer*) player)->pauseProcessing();
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_manager_PluginPlayer_playPreloadedAudioNative(JNIEnv *env, jobject thiz,
                                                                          jlong player) {
    ((aap::PluginPlayer*) player)->graph.playAudioData();
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_manager_PluginPlayer_setPluginNative(JNIEnv *env, jobject thiz,
                                                                 jlong player, jlong nativeClient,
                                                                 jint instanceId) {
    auto client = (aap::PluginClient*) nativeClient;
    auto instance = client->getInstanceById(instanceId);
    ((aap::PluginPlayer*) player)->graph.setPlugin((aap::RemotePluginInstance*) instance);
}
