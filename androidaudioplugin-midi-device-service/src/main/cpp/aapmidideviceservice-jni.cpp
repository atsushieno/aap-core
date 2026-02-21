
#include <jni.h>
#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>

#include <chrono>
#include <thread>

#include <aap/core/host/audio-plugin-host.h>
#include <aap/core/host/android/audio-plugin-host-android.h>
#include <aap/core/android/android-application-context.h>
#include <aap/unstable/logging.h>
#include "AAPMidiProcessor_android.h"

#define LOG_TAG_JNI "AAPMidiDeviceServiceJNI"

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

// ---------------------------------------------------------------------------
// MIDI output sender — routes CI responses back to the Android MIDI host.
// ---------------------------------------------------------------------------

static JavaVM* g_jvm{nullptr};
static jobject g_midi_output_callback{nullptr};  // global ref to Kotlin callback object
static jmethodID g_midi_output_send_method{nullptr};

// Retrieve or attach JNIEnv for the calling thread.
// Returns true if AttachCurrentThread was called (caller must detach).
static bool getJniEnv(JNIEnv** env) {
    if (!g_jvm) return false;
    jint status = g_jvm->GetEnv(reinterpret_cast<void**>(env), JNI_VERSION_1_6);
    if (status == JNI_OK) return false;           // already attached
    if (status == JNI_EDETACHED) {
        if (g_jvm->AttachCurrentThread(env, nullptr) != JNI_OK) return false;
        return true;                               // caller must detach
    }
    return false;
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
        JNIEnv *env, jobject midiReceiver,
        jint connectorInstanceId, jint sampleRate, jint oboeFrameSize, jint audioOutChannelCount,
        jint aapFrameSize, jint midiBufferSize, jint midiTransport) {
    startNewDeviceInstance();

    auto connections = aap::getPluginConnectionListByConnectorInstanceId(connectorInstanceId, true);
    if (!connections) {
        AAP_ASSERT_FALSE;
        return;
    }

    AAPMIDIDEVICE_INSTANCE->initialize(connections,
                                       sampleRate, oboeFrameSize, audioOutChannelCount,
                                       aapFrameSize, midiBufferSize, midiTransport);
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

/**
 * Register a Kotlin MidiOutputCallback object whose send(ByteArray, Int, Int, Long) method
 * will be called whenever the MIDI-CI session needs to write response bytes to the host.
 *
 * Pass null to unregister a previously registered callback.
 *
 * Expected Kotlin interface:
 *   fun interface MidiOutputCallback {
 *       fun send(data: ByteArray, offset: Int, count: Int, timestamp: Long)
 *   }
 */
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiDeviceInstance_setMidiOutputCallback(
        JNIEnv *env, jobject thiz, jobject callback) {
    // Store JavaVM on first call so we can re-attach from non-JNI threads.
    if (!g_jvm)
        env->GetJavaVM(&g_jvm);

    // Release any previously registered callback.
    if (g_midi_output_callback) {
        env->DeleteGlobalRef(g_midi_output_callback);
        g_midi_output_callback = nullptr;
        g_midi_output_send_method = nullptr;
    }

    if (!callback) {
        // Unregister: clear the sender in the processor.
        if (processor)
            processor->setMidiOutputSender({});
        return;
    }

    g_midi_output_callback = env->NewGlobalRef(callback);

    jclass cls = env->GetObjectClass(callback);
    g_midi_output_send_method = env->GetMethodID(cls, "send", "([BIIJ)V");
    if (!g_midi_output_send_method) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG_JNI,
                     "setMidiOutputCallback: could not find send([BIIJ)V on callback object");
        env->DeleteGlobalRef(g_midi_output_callback);
        g_midi_output_callback = nullptr;
        return;
    }

    // Register the native sender that bridges to the Kotlin callback.
    //
    // Mirrors LibreMidiIODevice::send() in UAPMD: send at most CHUNK_BYTES bytes at a
    // time and sleep SYSEX_DELAY_US microseconds between chunks so that the receiving
    // end (e.g. ktmidi-ci-tool) has time to process each fragment before the next one
    // arrives.  No trailing sleep is inserted after the final (possibly short) chunk.
    auto sender = [](const uint8_t* data, size_t offset, size_t length, uint64_t timestamp) {
        if (!g_midi_output_callback || !g_midi_output_send_method || !g_jvm)
            return;

        JNIEnv* env = nullptr;
        bool attached = getJniEnv(&env);
        if (!env) return;

        static constexpr size_t CHUNK_BYTES   = 256;
        static constexpr int    SYSEX_DELAY_US = 1000;

        size_t sent = 0;
        while (sent < length) {
            const size_t chunk = std::min(CHUNK_BYTES, length - sent);

            jbyteArray arr = env->NewByteArray(static_cast<jsize>(chunk));
            if (arr) {
                env->SetByteArrayRegion(arr, 0,
                                        static_cast<jsize>(chunk),
                                        reinterpret_cast<const jbyte*>(data + offset + sent));
                env->CallVoidMethod(g_midi_output_callback,
                                    g_midi_output_send_method,
                                    arr,
                                    static_cast<jint>(0),
                                    static_cast<jint>(chunk),
                                    static_cast<jlong>(timestamp));
                env->DeleteLocalRef(arr);
            }

            sent += chunk;
            // Pace delivery: sleep between chunks, but not after the last one.
            if (sent < length)
                std::this_thread::sleep_for(std::chrono::microseconds(SYSEX_DELAY_US));
        }

        if (attached)
            g_jvm->DetachCurrentThread();
    };

    if (processor)
        processor->setMidiOutputSender(std::move(sender));
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    g_jvm = vm;
    return JNI_VERSION_1_6;
}

} // extern "C"
