//
// Created by atsushi on 19/06/07.
//

#include "AAPRemoteBridge.h"
#include "../../../../../../include/android-audio-plugin-host.hpp"
#include <jni.h>
#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>
#include <android/binder_interface_utils.h>
#include <android/binder_parcel.h>
#include <android/binder_parcel_utils.h>
#include <android/binder_status.h>
#include <android/binder_auto_utils.h>
#include <android/log.h>

AIBinder_Class *binder_class;
AIBinder *binder;

namespace aap {

const char *interface_descriptor = "org.androidaudiopluginframework.AudioPluginService";


// TODO: any code that calls this method needs to implement proper memory management.
const char *strdup_fromJava(JNIEnv *env, jstring s) {
    jboolean isCopy;
    if (!s)
        return NULL;
    const char *ret = env->GetStringUTFChars(s, &isCopy);
    return isCopy ? ret : strdup(ret);
}

const char *java_plugin_information_class_name = "org/androidaudiopluginframework/PluginInformation",
        *java_port_information_class_name = "org/androidaudiopluginframework/PortInformation";

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
                                             "(I)Lorg/androidaudiopluginframework/PortInformation;");
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

} // namespace aap

extern "C" {

aap::PluginInformation **local_plugin_infos;

void Java_org_androidaudiopluginframework_hosting_AudioPluginHost_initialize(JNIEnv *env, jclass cls, jobjectArray jPluginInfos)
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

jobject
Java_org_androidaudiopluginframework_AudioPluginService_createBinder(JNIEnv *env, jclass clazz) {
    if (binder == NULL) {
        if (binder_class == NULL)
            binder_class = AIBinder_Class_define(aap::interface_descriptor, aap::aap_oncreate, aap::aap_ondestroy,
                                                 aap::aap_ontransact);
        binder = AIBinder_new(binder_class, NULL);
    }
    return AIBinder_toJavaBinder(env, binder);
}

void
Java_org_androidaudiopluginframework_AudioPluginService_destroyBinder(JNIEnv *env, jclass clazz,
                                                                      jobject binder) {
    auto abinder = AIBinder_fromJavaBinder(env, binder);
    AIBinder_decStrong(abinder);
}

} // extern "C"
