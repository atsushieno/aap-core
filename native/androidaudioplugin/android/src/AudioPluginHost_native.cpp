
#include <jni.h>
#include <android/log.h>
#include <sys/mman.h>
#include "aidl/org/androidaudioplugin/BnAudioPluginInterface.h"
#include "aidl/org/androidaudioplugin/BpAudioPluginInterface.h"
#include "aap/android-audio-plugin-host.hpp"
#include "AudioPluginInterfaceImpl.h"

std::unique_ptr<aidl::org::androidaudioplugin::BnAudioPluginInterface> sp_binder;

namespace aap {


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
        *java_port_information_class_name = "org/androidaudioplugin/PortInformation",
        *java_audio_plugin_host_helper_class_name = "org/androidaudioplugin/AudioPluginHostHelper";

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
        j_method_port_get_content,
        j_method_query_audio_plugins;

void initializeJNIMetadata(JNIEnv *env)
{
    jclass java_plugin_information_class = env->FindClass(java_plugin_information_class_name),
        java_port_information_class = env->FindClass(java_port_information_class_name),
        java_audio_plugin_host_helper_class = env->FindClass(java_audio_plugin_host_helper_class_name);

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
    j_method_query_audio_plugins = env->GetStaticMethodID(java_audio_plugin_host_helper_class, "queryAudioPlugins",
            "(Landroid/content/Context;)[Lorg/androidaudioplugin/PluginInformation;");
}

aap::PluginInformation *
pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation) {
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


JavaVM *jvm{nullptr};
jobject globalApplicationContext{nullptr};
std::vector<jobject> binders{};

JNIEnv *getJNIEnv()
{
    assert(jvm != nullptr);

    JNIEnv* env;
    jvm->AttachCurrentThread(&env, nullptr);
    return env;
}

// FIXME: I haven't figured out how it results in crashes yet. Plugins must be set in Kotlin layer so far.
jobjectArray queryInstalledPluginsJNI()
{
    auto env = getJNIEnv();
    jclass java_audio_plugin_host_helper_class = env->FindClass(java_audio_plugin_host_helper_class_name);
    j_method_query_audio_plugins = env->GetStaticMethodID(java_audio_plugin_host_helper_class, "queryAudioPlugins",
                                                          "(Landroid/content/Context;)[Lorg/androidaudioplugin/PluginInformation;");
    return (jobjectArray) env->CallStaticObjectMethod(java_audio_plugin_host_helper_class, j_method_query_audio_plugins, globalApplicationContext);
}


void cleanupKnownPlugins()
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

aap::PluginInformation** convertPluginList(jobjectArray jPluginInfos)
{
    assert(jPluginInfos != nullptr);
    auto localPlugins = aap::getKnownPluginInfos();
    if(localPlugins)
        cleanupKnownPlugins();
    auto env = getJNIEnv();
    jsize infoSize = env->GetArrayLength(jPluginInfos);
    localPlugins = (aap::PluginInformation **) calloc(sizeof(aap::PluginInformation *), infoSize + 1);
    for (int i = 0; i < infoSize; i++) {
        auto jPluginInfo = (jobject) env->GetObjectArrayElement(jPluginInfos, i);
        localPlugins[i] = aap::pluginInformation_fromJava(env, jPluginInfo);
    }
    localPlugins[infoSize] = nullptr;
    return localPlugins;
}

void initializeKnownPlugins(jobjectArray jPluginInfos = nullptr)
{
    jPluginInfos = jPluginInfos != nullptr ? jPluginInfos : queryInstalledPluginsJNI();
    aap::setKnownPluginInfos(convertPluginList(jPluginInfos));
}

aap::PluginInformation** queryInstalledPlugins()
{
    return convertPluginList(queryInstalledPluginsJNI());
}

class AndroidPluginHostPAL : public PluginHostPAL
{
public:
    PluginInformation** getInstalledPlugins() override {
        return queryInstalledPlugins();
    }
};

AndroidPluginHostPAL android_pal_instance{};

PluginHostPAL* getPluginHostPAL()
{
    return &android_pal_instance;
}

}

extern "C" {

jobject
Java_org_androidaudioplugin_AudioPluginNatives_createBinderForService(JNIEnv *env, jclass clazz, jint sampleRate) {
    sp_binder.reset(new aap::AudioPluginInterfaceImpl(sampleRate));
    auto ret = AIBinder_toJavaBinder(env, sp_binder->asBinder().get());
    return ret;
}

void
Java_org_androidaudioplugin_AudioPluginNatives_destroyBinderForService(JNIEnv *env, jclass clazz,
                                                                      jobject binder) {
    auto abinder = AIBinder_fromJavaBinder(env, binder);
    AIBinder_decStrong(abinder);
    sp_binder.reset(nullptr);
}

void Java_org_androidaudioplugin_AudioPluginNatives_initializeLocalHost(JNIEnv *env, jclass cls, jobjectArray jPluginInfos)
{
    // FIXME: enable later code once queryInstalledPluginsJNI() is fixed.
    assert(jPluginInfos != nullptr);
	//if (jPluginInfos == nullptr)
	//	jPluginInfos = aap::queryInstalledPluginsJNI();
    aap::initializeKnownPlugins(jPluginInfos);
}

void Java_org_androidaudioplugin_AudioPluginNatives_cleanupLocalHostNatives(JNIEnv *env, jclass cls)
{
	aap::cleanupKnownPlugins();
}

JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_setApplicationContext(JNIEnv *env, jclass clazz,
																  jobject applicationContext) {
	env->GetJavaVM(&aap::jvm);
    aap::globalApplicationContext = applicationContext;
    aap::initializeJNIMetadata(env);
}
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_initialize(JNIEnv *env, jclass clazz,
                                                       jobjectArray jPluginInfos) {
    assert(aap::getJNIEnv() == env);
    aap::initializeKnownPlugins(jPluginInfos);
}

JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_addBinderForHost(JNIEnv *env, jclass clazz,
                                                                jobject binder) {
    aap::binders.push_back(binder);
}

JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_removeBinderForHost(JNIEnv *env, jclass clazz,
                                                                jobject binder) {
    aap::binders.erase(std::find(aap::binders.begin(), aap::binders.end(), binder));
}

JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_instantiatePlugin(JNIEnv *env, jclass clazz,
																   jstring serviceIdentifier, jstring pluginId) {
	__android_log_print(ANDROID_LOG_DEBUG, "AAPNativeBridge", "TODO: implement instantiatePlugin");
}

} // extern "C"
