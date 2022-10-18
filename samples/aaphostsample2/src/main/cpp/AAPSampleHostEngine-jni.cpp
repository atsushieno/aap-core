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
std::string fromJavaString(JNIEnv *env, jstring s) {
    jboolean isCopy;
    if (!s)
        return "";
    const char *u8 = env->GetStringUTFChars(s, &isCopy);
    std::string ret{u8};
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
        JNIEnv *env, jobject kotlinEngineObj, jint connectorInstanceId, jint sampleRate, jint oboeFrameSize, jint audioOutChannelCount, jint aapFrameSize) {
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
    auto pluginIdSStr = fromJavaString(env, pluginId);

    engine->instantiatePlugin(pluginIdSStr);
}

jbyte jni_midi_buffer[1024]{};

JNIEXPORT void JNICALL Java_org_androidaudioplugin_samples_aaphostsample2_PluginHostEngine_processMessage(
        JNIEnv *env, jobject midiReceiver, jbyteArray bytes, jint offset, jint length,
        jlong timestampInNanoseconds) {
    env->GetByteArrayRegion(bytes, offset, length, jni_midi_buffer);
    engine->processMidiInput(
            reinterpret_cast<uint8_t *>(jni_midi_buffer), 0, length, timestampInNanoseconds);
}

} // extern "C"
