
#include <jni.h>
#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>
#include <android/binder_interface_utils.h>
#include <android/binder_parcel.h>
#include <android/binder_parcel_utils.h>
#include <android/binder_status.h>
#include <android/binder_auto_utils.h>
#include <android/log.h>
#include "aidl/org/androidaudioplugin/BnAudioPluginInterface.h"
#include "aidl/org/androidaudioplugin/BpAudioPluginInterface.h"
#include "aap/android-audio-plugin-host.hpp"

aidl::org::androidaudioplugin::BnAudioPluginInterface *sp_binder;

extern "C" {
extern aap::PluginInformation **local_plugin_infos;
}

// FIXME: sort out final library header structures.
namespace aap {
    extern aap::PluginInformation *pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation);
    extern const char *strdup_fromJava(JNIEnv *env, jstring s);
}

namespace aap {

const char *interface_descriptor = "org.androidaudioplugin.AudioPluginService";


class AudioPluginInterfaceImpl : public aidl::org::androidaudioplugin::BnAudioPluginInterface {
    aap::PluginHost *host;
    aap::PluginInstance *instance;
    AndroidAudioPluginBuffer buffer;
    int sample_rate;

public:

    AudioPluginInterfaceImpl(int sampleRate) : sample_rate(sampleRate)
    {
        host = new PluginHost(local_plugin_infos);
        buffer.num_frames = 0;
        buffer.buffers = nullptr;
    }

    ::ndk::ScopedAStatus create(const std::string& in_pluginId, int32_t in_sampleRate) override
    {
        if (instance)
            return ndk::ScopedAStatus(AStatus_fromServiceSpecificErrorWithMessage(-1, "Already instantiated"));
        instance = host->instantiatePlugin(in_pluginId.data());
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus isPluginAlive(bool* _aidl_return) override
    {
        *_aidl_return = true;
        return ndk::ScopedAStatus::ok();
    }

    void resetBuffers(int frameCount, const std::vector<int64_t>& pointers)
    {
        buffer.num_frames = frameCount;
        int n = pointers.size();
        if (buffer.buffers)
            free(buffer.buffers);
        buffer.buffers = (void**) calloc(sizeof(void*), n);
        for (int i = 0; i < n; i++)
            buffer.buffers[i] = (void*) pointers[i];
    }

    ::ndk::ScopedAStatus prepare(int32_t in_frameCount, int32_t in_bufferCount, const std::vector<int64_t>& in_bufferPointers) override
    {
        resetBuffers(in_bufferCount, in_bufferPointers);
        instance->prepare(sample_rate, in_frameCount, &buffer);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus activate() override
    {
        instance->activate();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus process(int32_t in_timeoutInNanoseconds) override
    {
        instance->process(&buffer, in_timeoutInNanoseconds);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus deactivate() override
    {
        instance->deactivate();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus getStateSize(int32_t* _aidl_return) override
    {
        *_aidl_return = instance->getStateSize();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus getState(int64_t in_pointer) override
    {
        void* dst = (void*) in_pointer;
        memcpy(dst, instance->getState(), instance->getStateSize());
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus setState(int64_t in_pointer, int32_t in_size) override
    {
        void* src = (void*) in_pointer;
        instance->setState(src, 0, in_size);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus destroy() override
    {
        instance->dispose();
        return ndk::ScopedAStatus::ok();
    }
};


void* aap_oncreate(void* args)
{
    __android_log_print(ANDROID_LOG_DEBUG, "AAPNativeBridge", "aap_oncreate");
    return NULL;
}

void aap_ondestroy(void* userData)
{
    __android_log_print(ANDROID_LOG_DEBUG, "AAPNativeBridge", "aap_ondestroy");
}

binder_status_t aap_ontransact(AIBinder *binder, transaction_code_t code, const AParcel *in, AParcel *out)
{
    __android_log_print(ANDROID_LOG_DEBUG, "AAPNativeBridge", "aap_ontransact");
    return STATUS_OK;
}

// TODO: any code that calls this method needs to implement proper memory management.
const char *strdup_fromJava(JNIEnv *env, jstring s) {
    jboolean isCopy;
    if (!s)
        return NULL;
    const char *ret = env->GetStringUTFChars(s, &isCopy);
    return isCopy ? ret : strdup(ret);
}

const char *java_plugin_information_class_name = "org/androidaudioplugin/PluginInformation",
        *java_port_information_class_name = "org/androidaudioplugin/PortInformation";

static jclass java_plugin_information_class, java_port_information_class;
static jmethodID
        j_method_is_out_process,
        j_method_get_name,
        j_method_get_manufacturer,
        j_method_get_version,
        j_method_get_plugin_id,
        j_method_get_shared_library_filename,
        j_method_get_library_entrypoint,
        j_method_get_port_count,
        j_method_get_port,
        j_method_port_get_name,
        j_method_port_get_direction,
        j_method_port_get_content;

aap::PluginInformation *
pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation) {
    if (!java_plugin_information_class) {
        java_plugin_information_class = env->FindClass(java_plugin_information_class_name);
        java_port_information_class = env->FindClass(java_port_information_class_name);
        j_method_is_out_process = env->GetMethodID(java_plugin_information_class,
                                                   "isOutProcess", "()Z");
        j_method_get_name = env->GetMethodID(java_plugin_information_class, "getName",
                                             "()Ljava/lang/String;");
        j_method_get_manufacturer = env->GetMethodID(java_plugin_information_class,
                                                     "getManufacturer", "()Ljava/lang/String;");
        j_method_get_version = env->GetMethodID(java_plugin_information_class, "getVersion",
                                                "()Ljava/lang/String;");
        j_method_get_plugin_id = env->GetMethodID(java_plugin_information_class, "getPluginId",
                                                  "()Ljava/lang/String;");
        j_method_get_shared_library_filename = env->GetMethodID(java_plugin_information_class,
                                                                "getSharedLibraryName",
                                                                "()Ljava/lang/String;");
        j_method_get_library_entrypoint = env->GetMethodID(java_plugin_information_class,
                                                           "getLibraryEntryPoint",
                                                           "()Ljava/lang/String;");
        j_method_get_port_count = env->GetMethodID(java_plugin_information_class,
                                                   "getPortCount", "()I");
        j_method_get_port = env->GetMethodID(java_plugin_information_class, "getPort",
                                             "(I)Lorg/androidaudioplugin/PortInformation;");
        j_method_port_get_name = env->GetMethodID(java_port_information_class, "getName",
                                                  "()Ljava/lang/String;");
        j_method_port_get_direction = env->GetMethodID(java_port_information_class,
                                                       "getDirection", "()I");
        j_method_port_get_content = env->GetMethodID(java_port_information_class, "getContent",
                                                     "()I");
    }
    auto aapPI = new aap::PluginInformation(
            env->CallBooleanMethod(pluginInformation, j_method_is_out_process),
            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                 j_method_get_name)),
            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                 j_method_get_manufacturer)),
            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                 j_method_get_version)),
            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                 j_method_get_plugin_id)),
            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                 j_method_get_shared_library_filename)),
            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                 j_method_get_library_entrypoint))
    );

    int nPorts = env->CallIntMethod(pluginInformation, j_method_get_port_count);
    for (int i = 0; i < nPorts; i++) {
        jobject port = env->CallObjectMethod(pluginInformation, j_method_get_port, i);
        auto aapPort = new aap::PortInformation(strdup_fromJava(env,
                                                                (jstring) env->CallObjectMethod(
                                                                        port,
                                                                        j_method_port_get_name)),
                                                (aap::ContentType) (int) env->CallIntMethod(
                                                        port, j_method_port_get_content),
                                                (aap::PortDirection) (int) env->CallIntMethod(
                                                        port, j_method_port_get_direction));
        aapPI->addPort(aapPort);
    }

    return aapPI;
}

}


// FIXME: sample app code, should be moved elsewhere

namespace aapremote {

int runClientAAP(AIBinder *binder, int sampleRate, const aap::PluginInformation *pluginInfo, void *wav, int wavLength, void *outWav) {
    auto proxy = new aidl::org::androidaudioplugin::BpAudioPluginInterface(ndk::SpAIBinder(binder));

    int buffer_size = wavLength;
    int float_count = buffer_size / sizeof(float);

    /* instantiate plugins and connect ports */

    float *audioIn = (float *) calloc(buffer_size, 1);
    float *midiIn = (float *) calloc(buffer_size, 1);
    float *controlIn = (float *) calloc(buffer_size, 1);
    float *dummyBuffer = (float *) calloc(buffer_size, 1);

    float *currentAudioIn = audioIn, *currentAudioOut = NULL, *currentMidiIn = midiIn, *currentMidiOut = NULL;

    auto status = proxy->create(pluginInfo->getPluginID(), sampleRate);
    assert (status.isOk());

    auto desc = pluginInfo;
    auto plugin_buffer = new AndroidAudioPluginBuffer();
    plugin_buffer->num_frames = buffer_size / sizeof(float);
    int nPorts = desc->getNumPorts();
    plugin_buffer->buffers = (void **) calloc(nPorts + 1, sizeof(void *));
    for (int p = 0; p < nPorts; p++) {
        auto port = desc->getPort(p);
        if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT &&
            port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO)
            plugin_buffer->buffers[p] = currentAudioIn;
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO)
            plugin_buffer->buffers[p] = currentAudioOut = (float *) calloc(buffer_size, 1);
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI)
            plugin_buffer->buffers[p] = currentMidiIn;
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI)
            plugin_buffer->buffers[p] = currentMidiOut = (float *) calloc(buffer_size, 1);
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT)
            plugin_buffer->buffers[p] = controlIn;
        else
            plugin_buffer->buffers[p] = dummyBuffer;
    }
    if (currentAudioOut)
        currentAudioIn = currentAudioOut;
    if (currentMidiOut)
        currentMidiIn = currentMidiOut;

    // prepare connections
    std::vector<int64_t> buffer_proxy;
    for(int i = 0; i < nPorts; i++)
        buffer_proxy.push_back((int64_t) plugin_buffer->buffers[i]);

    proxy->prepare(plugin_buffer->num_frames, nPorts, buffer_proxy);

    // prepare inputs
    memcpy(audioIn, wav, buffer_size);
    for (int i = 0; i < float_count; i++)
        controlIn[i] = 0.5;

    // activate, run, deactivate
    proxy->activate();
    proxy->process(0);
    proxy->deactivate();

    memcpy(outWav, currentAudioOut, buffer_size);

    for (int p = 0; plugin_buffer->buffers[p]; p++)
        free(plugin_buffer->buffers[p]);
    free(plugin_buffer->buffers);
    delete plugin_buffer;

    proxy->destroy();

    delete proxy;

    free(audioIn);
    free(midiIn);
    free(dummyBuffer);

    return 0;
}

} // namespace aapremote


extern "C" {

jobject
Java_org_androidaudioplugin_AudioPluginService_createBinder(JNIEnv *env, jclass clazz, jint sampleRate, jstring pluginId) {
    sp_binder = new aap::AudioPluginInterfaceImpl(sampleRate);
    return AIBinder_toJavaBinder(env, sp_binder->asBinder().get());
}

void
Java_org_androidaudioplugin_AudioPluginService_destroyBinder(JNIEnv *env, jclass clazz,
                                                                      jobject binder) {
    auto abinder = AIBinder_fromJavaBinder(env, binder);
    AIBinder_decStrong(abinder);
}

void Java_org_androidaudioplugin_AudioPluginHost_initialize(JNIEnv *env, jclass cls, jobjectArray jPluginInfos)
{
    if (!local_plugin_infos) {
        jsize infoSize = env->GetArrayLength(jPluginInfos);
        local_plugin_infos = (aap::PluginInformation **) calloc(sizeof(aap::PluginInformation *), infoSize);
        for (int i = 0; i < infoSize; i++) {
            auto jPluginInfo = (jobject) env->GetObjectArrayElement(jPluginInfos, i);
            local_plugin_infos[i] = aap::pluginInformation_fromJava(env, jPluginInfo);
        }
    }
}

void Java_org_androidaudioplugin_AudioPluginHost_cleanup(JNIEnv *env, jclass cls)
{
    int n = 0;
    while (local_plugin_infos[n])
        n++;
    for(int i = 0; i < n; i++)
        delete local_plugin_infos[i];
}

int Java_org_androidaudioplugin_AudioPluginHost_runClientAAP(JNIEnv *env, jclass cls, jobject jBinder, jint sampleRate, jstring jPluginId, jbyteArray wav, jbyteArray outWav) {

    int wavLength = env->GetArrayLength(wav);
    void *wavBytes = calloc(wavLength, 1);
    env->GetByteArrayRegion(wav, 0, wavLength, (jbyte *) wavBytes);
    void *outWavBytes = calloc(wavLength, 1);

    jboolean dup;
    const char *pluginId = env->GetStringUTFChars(jPluginId, &dup);
    aap::PluginInformation *pluginInfo;
    int p = 0;
    while (local_plugin_infos[p]) {
        if (strcmp(local_plugin_infos[p]->getPluginID(), pluginId) == 0) {
            pluginInfo = local_plugin_infos[p];
            break;
        }
    }

    auto binder = AIBinder_fromJavaBinder(env, jBinder);
    int ret = aapremote::runClientAAP(binder, sampleRate, pluginInfo, wavBytes, wavLength, outWavBytes);

    env->SetByteArrayRegion(outWav, 0, wavLength, (jbyte*) outWavBytes);

    free(wavBytes);
    free(outWavBytes);
    return ret;
}

} // extern "C"
