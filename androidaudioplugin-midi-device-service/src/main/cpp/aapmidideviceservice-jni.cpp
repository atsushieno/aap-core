
#include <jni.h>
#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>

#include <aap/core/host/audio-plugin-host.h>
#include <aap/core/host/android/audio-plugin-host-android.h>
#include <aap/core/android/android-application-context.h>
#include <aap/unstable/logging.h>
#include "AAPMidiProcessor_android.h"


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

extern "C" {

aap::midi::AAPMidiProcessorAndroid *processor{nullptr};

aap::midi::AudioDriverType driver_type{
    aap::midi::AudioDriverType::AAP_MIDI_PROCESSOR_AUDIO_DRIVER_TYPE_OBOE};

void startNewDeviceInstance() {
    processor = new aap::midi::AAPMidiProcessorAndroid(driver_type);
}

aap::midi::AAPMidiProcessorAndroid* getDeviceInstance() {
    if (!processor)
        startNewDeviceInstance();
    return processor;
}

#define AAPMIDIDEVICE_INSTANCE getDeviceInstance()

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiDeviceInstance_initializeMidiProcessor(
        JNIEnv *env, jobject midiReceiver, jint connectorInstanceId, jint sampleRate, jint oboeFrameSize, jint audioOutChannelCount, jint aapFrameSize, jint midiBufferSize) {
    startNewDeviceInstance();

    auto connections = aap::getPluginConnectionListByConnectorInstanceId(connectorInstanceId, true);
    if (!connections) {
        AAP_ASSERT_FALSE;
        return;
    }

    AAPMIDIDEVICE_INSTANCE->initialize(connections, sampleRate, oboeFrameSize, audioOutChannelCount, aapFrameSize, midiBufferSize);
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiDeviceInstance_terminateMidiProcessor(
        JNIEnv *env, jobject midiReceiver) {
    AAPMIDIDEVICE_INSTANCE->terminate();

    processor = nullptr;

    aap::aprintf("AudioPluginMidiReceiver terminated.");
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiDeviceInstance_activate(
        JNIEnv *env, jobject midiReceiver) {
    AAPMIDIDEVICE_INSTANCE->activate();
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiDeviceInstance_deactivate(
        JNIEnv *env, jobject midiReceiver) {
    AAPMIDIDEVICE_INSTANCE->deactivate();
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiDeviceInstance_instantiatePlugin(
        JNIEnv *env, jobject midiReceiver, jstring pluginId) {
    auto pluginIdPtr = dupFromJava(env, pluginId);
    std::string pluginIdString = pluginIdPtr;

    AAPMIDIDEVICE_INSTANCE->instantiatePlugin(pluginIdString);

    free((void *) pluginIdPtr);
}

jbyte jni_midi_buffer[1024]{};

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiDeviceInstance_processMessage(
        JNIEnv *env, jobject midiReceiver, jbyteArray bytes, jint offset, jint length,
        jlong timestampInNanoseconds) {
    if (length > 1024) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiDeviceServiceJNI", "MIDI input too long, it should be within %d bytes, but % bytes were passed.", sizeof(jni_midi_buffer), length);
        return;
    }
    env->GetByteArrayRegion(bytes, offset, length, jni_midi_buffer);
    AAPMIDIDEVICE_INSTANCE->processMidiInput(
            reinterpret_cast<uint8_t *>(jni_midi_buffer), 0, length, timestampInNanoseconds);
}

JNIEXPORT void JNICALL
Java_org_androidaudioplugin_midideviceservice_MidiDeviceServiceStartup_initializeApplicationContext(
        JNIEnv *env, jobject thiz, jobject context) {
    // Should we actually replace this with private field in this libaapmidideviceservice.so?
    aap::set_application_context(env, context);
}

} // extern "C"
