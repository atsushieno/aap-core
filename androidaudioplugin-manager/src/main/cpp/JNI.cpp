#include <jni.h>

#include "PluginPlayer.h"

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_manager_PluginPlayer_loadAudioResourceNative(JNIEnv *env, jobject thiz,
                                                                         jlong native,
                                                                         jbyteArray bytes,
                                                                         jstring filename) {
    // TODO: implement loadAudioResourceNative()
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_manager_PluginPlayer_addMidiEventNative(JNIEnv *env, jobject thiz,
                                                                    jlong native, jbyteArray bytes,
                                                                    jint offset, jint length) {
    // TODO: implement addMidiEventNative()
}

extern "C"
JNIEXPORT jlong JNICALL
Java_org_androidaudioplugin_manager_PluginPlayer_00024Companion_createNewPluginPlayer(JNIEnv *env,
                                                                                      jobject thiz,
                                                                                      int32_t sampleRate,
                                                                                      int32_t framesPerCallback,
                                                                                      jlong nativeClient,
                                                                                      jint instanceId) {
    aap::PluginPlayerConfiguration configuration{sampleRate, framesPerCallback};
    auto client = (aap::PluginClient*) nativeClient;
    auto instance = client->getInstanceById(instanceId);
    return (jlong) (void*) new aap::PluginPlayer(configuration, (aap::RemotePluginInstance*) instance);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_manager_PluginPlayer_deletePluginPlayer(JNIEnv *env, jobject thiz,
                                                                    jlong native) {
    delete (aap::PluginPlayer*) native;
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_manager_PluginPlayer_setTargetInstanceNative(JNIEnv *env, jobject thiz,
                                                                         jlong native,
                                                                         jint instance_id) {
    // TODO: implement setTargetInstanceNative()
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_manager_PluginPlayer_startProcessingNative(JNIEnv *env, jobject thiz,
                                                                       jlong native) {
    // TODO: implement startProcessingNative()
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_manager_PluginPlayer_pauseProcessingNative(JNIEnv *env, jobject thiz,
                                                                       jlong native) {
    // TODO: implement startProcessingNative()
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_manager_PluginPlayer_playPreloadedAudioNative(JNIEnv *env, jobject thiz,
                                                                          jlong native) {
    // TODO: implement playPreloadedAudioNative()
}
