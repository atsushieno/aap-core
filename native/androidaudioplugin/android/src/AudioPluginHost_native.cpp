
#include <jni.h>
#include <android/log.h>
#include <sys/mman.h>
#include "aidl/org/androidaudioplugin/BnAudioPluginInterface.h"
#include "aidl/org/androidaudioplugin/BpAudioPluginInterface.h"
#include "aap/android-audio-plugin-host.hpp"
#include "AudioPluginInterfaceImpl.h"

aidl::org::androidaudioplugin::BnAudioPluginInterface *sp_binder;

namespace aap {

JavaVM *jvm;
jobject globalApplicationContext;
const char *interface_descriptor = "org.androidaudioplugin.AudioPluginService";


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

void initializeKnownPlugins(JNIEnv *env, jobjectArray jPluginInfos)
{
    auto localPlugins = aap::getKnownPluginInfos();
    if(localPlugins)
    	free(localPlugins); // FIXME: free plugin information list too. The list should be managed by host.
    jsize infoSize = env->GetArrayLength(jPluginInfos);
    localPlugins = (aap::PluginInformation **) calloc(sizeof(aap::PluginInformation *), infoSize + 1);
    for (int i = 0; i < infoSize; i++) {
        auto jPluginInfo = (jobject) env->GetObjectArrayElement(jPluginInfos, i);
        localPlugins[i] = aap::pluginInformation_fromJava(env, jPluginInfo);
    }
    localPlugins[infoSize] = nullptr;
    aap::setKnownPluginInfos(localPlugins);
}

void Java_org_androidaudioplugin_AudioPluginLocalHost_initialize(JNIEnv *env, jclass cls, jobjectArray jPluginInfos)
{
    initializeKnownPlugins(env, jPluginInfos);
}

void Java_org_androidaudioplugin_AudioPluginLocalHost_cleanupNatives(JNIEnv *env, jclass cls)
{
    auto localPlugins = aap::getKnownPluginInfos();
    assert(localPlugins != nullptr);
    int n = 0;
    while (localPlugins[n])
        n++;
    for(int i = 0; i < n; i++)
        delete localPlugins[i];
    localPlugins = nullptr;
}

JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginHost_setApplicationContext(JNIEnv *env, jclass clazz,
																  jobject applicationContext) {
	env->GetJavaVM(&aap::jvm);
    aap::globalApplicationContext = applicationContext;
}
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginHost_initialize(JNIEnv *env, jclass clazz,
                                                       jobjectArray jPluginInfos) {
    initializeKnownPlugins(env, jPluginInfos);
}

} // extern "C"
