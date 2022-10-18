// The large amount of code is copied from aapmidideviceservice-jni.cpp.

#include <jni.h>
#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>

#include <aap/core/host/audio-plugin-host.h>
#include <aap/core/host/android/audio-plugin-host-android.h>
#include <aap/core/host/android/android-application-context.h>
#include <aap/unstable/logging.h>
#include "AAPSampleHostEngine_android.h"

using namespace aap::host_engine;

// JNI entrypoints

// This returns std::string by value. Do not use for large chunk of strings.
const char* dupFromJava(JNIEnv *env, jstring s) {
    jboolean isCopy;
    if (!s)
        return "";
    const char *u8 = env->GetStringUTFChars(s, &isCopy);
    auto ret = strdup(u8);
    if (isCopy)
        env->ReleaseStringUTFChars(s, u8);
    return ret;
}

std::unique_ptr<AAPHostEngineBase> engine{nullptr};

extern "C" {

aap::host_engine::AudioDriverType driver_type{ AAP_HOST_ENGINE_AUDIO_DRIVER_TYPE_OBOE };

void startNewHostEngine() {
    engine = std::make_unique<AAPHostEngineAndroid>(driver_type);
}

AAPHostEngineBase* getHostEngine() {
    return engine.get();
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_samples_aaphostsample2_PluginHostEngine_initializeEngine(
        JNIEnv *env, jobject kotlinEngineObj, jint connectorInstanceId, jint sampleRate, jint audioOutChannelCount, jint aapFrameSize) {
    startNewHostEngine();

    auto connections = getPluginConnectionListFromJni(connectorInstanceId, true);
    assert(connections);

    engine->initialize(connections, sampleRate, audioOutChannelCount, aapFrameSize);
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_samples_aaphostsample2_PluginHostEngine_terminateEngine(
        JNIEnv *env, jobject kotlinEngineObj) {
    engine->terminate();

    engine.reset(nullptr);
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_samples_aaphostsample2_PluginHostEngine_activatePlugin(
        JNIEnv *env, jobject kotlinEngineObj) {
    engine->activate();
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_samples_aaphostsample2_PluginHostEngine_deactivatePlugin(
        JNIEnv *env, jobject kotlinEngineObj) {
    engine->deactivate();
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_samples_aaphostsample2_PluginHostEngine_instantiatePlugin(
        JNIEnv *env, jobject kotlinEngineObj, jstring pluginId) {
    auto pluginIdPtr = dupFromJava(env, pluginId);
    std::string pluginIdString = pluginIdPtr;

    engine->instantiatePlugin(pluginIdString);

    free((void *) pluginIdPtr);
}

} // extern "C"
