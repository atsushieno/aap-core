
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
        instance = nullptr;
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
    const char *u8 = env->GetStringUTFChars(s, &isCopy);
    auto ret = strdup(u8);
    env->ReleaseStringUTFChars(s, u8);
    return ret;
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

extern "C" {

jobject
Java_org_androidaudioplugin_AudioPluginService_createBinder(JNIEnv *env, jclass clazz, jint sampleRate) {
    sp_binder = new aap::AudioPluginInterfaceImpl(sampleRate);
    return AIBinder_toJavaBinder(env, sp_binder->asBinder().get());
}

void
Java_org_androidaudioplugin_AudioPluginService_destroyBinder(JNIEnv *env, jclass clazz,
                                                                      jobject binder) {
    auto abinder = AIBinder_fromJavaBinder(env, binder);
    AIBinder_decStrong(abinder);
    delete sp_binder;
}

void Java_org_androidaudioplugin_AudioPluginHost_initialize(JNIEnv *env, jclass cls, jobjectArray jPluginInfos)
{
    if (!local_plugin_infos) {
        jsize infoSize = env->GetArrayLength(jPluginInfos);
        local_plugin_infos = (aap::PluginInformation **) calloc(sizeof(aap::PluginInformation *), infoSize + 1);
        for (int i = 0; i < infoSize; i++) {
            auto jPluginInfo = (jobject) env->GetObjectArrayElement(jPluginInfos, i);
            local_plugin_infos[i] = aap::pluginInformation_fromJava(env, jPluginInfo);
        }
        local_plugin_infos[infoSize] = nullptr;
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

} // extern "C"
