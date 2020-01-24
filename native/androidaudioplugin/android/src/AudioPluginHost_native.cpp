
#include <jni.h>
#include <android/log.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "aidl/org/androidaudioplugin/BnAudioPluginInterface.h"
#include "aidl/org/androidaudioplugin/BpAudioPluginInterface.h"
#include "aap/android-audio-plugin-host.hpp"
#include "android-audio-plugin-host-android.hpp"
#include "AudioPluginInterfaceImpl.h"

extern "C" {

const char *strdup_fromJava(JNIEnv *env, jstring s) {
	jboolean isCopy;
	if (!s)
		return "";
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
		j_method_get_service_identifier,
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
		j_method_port_has_value_range,
		j_method_port_get_default,
		j_method_port_get_minimum,
		j_method_port_get_maximum,
		j_method_query_audio_plugins;

void initializeJNIMetadata(JNIEnv *env)
{
	jclass java_plugin_information_class = env->FindClass(java_plugin_information_class_name),
			java_port_information_class = env->FindClass(java_port_information_class_name),
			java_audio_plugin_host_helper_class = env->FindClass(java_audio_plugin_host_helper_class_name);

	j_method_is_out_process = env->GetMethodID(java_plugin_information_class,
											   "isOutProcess", "()Z");
	j_method_get_service_identifier = env->GetMethodID(java_plugin_information_class, "getServiceIdentifier",
													   "()Ljava/lang/String;");
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
	j_method_port_has_value_range = env->GetMethodID(java_port_information_class, "getHasValueRange",
												 "()Z");
	j_method_port_get_default = env->GetMethodID(java_port_information_class, "getDefault",
													 "()F");
	j_method_port_get_minimum = env->GetMethodID(java_port_information_class, "getMinimum",
												 "()F");
	j_method_port_get_maximum = env->GetMethodID(java_port_information_class, "getMaximum",
												 "()F");
	j_method_query_audio_plugins = env->GetStaticMethodID(java_audio_plugin_host_helper_class, "queryAudioPlugins",
														  "(Landroid/content/Context;)[Lorg/androidaudioplugin/PluginInformation;");
}

aap::PluginInformation *
pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation) {
	auto aapPI = new aap::PluginInformation(
			env->CallBooleanMethod(pluginInformation, j_method_is_out_process),
			strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
																 j_method_get_service_identifier)),
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
		auto name = strdup_fromJava(env, (jstring) env->CallObjectMethod(port, j_method_port_get_name));
		auto content = (aap::ContentType) (int) env->CallIntMethod(port, j_method_port_get_content);
		auto direction = (aap::PortDirection) (int) env->CallIntMethod(port, j_method_port_get_direction);
		aapPI->addPort(
			env->CallBooleanMethod(port, j_method_port_has_value_range) ?
			new aap::PortInformation(name, content, direction,
					env->CallFloatMethod(port, j_method_port_get_default),
					env->CallFloatMethod(port, j_method_port_get_minimum),
					env->CallFloatMethod(port, j_method_port_get_maximum)) :
			new aap::PortInformation(name, content, direction));
	}

	return aapPI;
}


// FIXME: I haven't figured out how it results in crashes yet. Plugins must be set in Kotlin layer so far.
jobjectArray queryInstalledPluginsJNI()
{
	auto apal = dynamic_cast<aap::AndroidPluginHostPAL*>(aap::getPluginHostPAL());
	auto env = apal->getJNIEnv();
	jclass java_audio_plugin_host_helper_class = env->FindClass(java_audio_plugin_host_helper_class_name);
	j_method_query_audio_plugins = env->GetStaticMethodID(java_audio_plugin_host_helper_class, "queryAudioPlugins",
														  "(Landroid/content/Context;)[Lorg/androidaudioplugin/PluginInformation;");
	return (jobjectArray) env->CallStaticObjectMethod(java_audio_plugin_host_helper_class, j_method_query_audio_plugins, apal->globalApplicationContext);
}


std::unique_ptr<aidl::org::androidaudioplugin::BnAudioPluginInterface> sp_binder;

jobject
Java_org_androidaudioplugin_AudioPluginNatives_createBinderForService(JNIEnv *env, jclass clazz, jint sampleRate) {
    sp_binder.reset(new aap::AudioPluginInterfaceImpl());
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
	auto apal = dynamic_cast<aap::AndroidPluginHostPAL*>(aap::getPluginHostPAL());
	apal->initializeKnownPlugins(jPluginInfos);
}

void Java_org_androidaudioplugin_AudioPluginNatives_cleanupLocalHostNatives(JNIEnv *env, jclass cls)
{
	auto apal = dynamic_cast<aap::AndroidPluginHostPAL*>(aap::getPluginHostPAL());
	apal->cleanupKnownPlugins();
}

JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_setApplicationContext(JNIEnv *env, jclass clazz,
																  jobject applicationContext) {
	auto apal = dynamic_cast<aap::AndroidPluginHostPAL*>(aap::getPluginHostPAL());
	apal->initialize(env, applicationContext);
    initializeJNIMetadata(env);
}
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_initialize(JNIEnv *env, jclass clazz,
                                                       jobjectArray jPluginInfos) {
	auto apal = dynamic_cast<aap::AndroidPluginHostPAL*>(aap::getPluginHostPAL());
    apal->initializeKnownPlugins(jPluginInfos);
}

JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_addBinderForHost(JNIEnv *env, jclass clazz,
                                                                jstring serviceIdentifier, jobject binder) {
	const char *serviceIdentifierDup = strdup_fromJava(env, serviceIdentifier);
	auto binderRef = env->NewGlobalRef(binder);
	auto aiBinder = AIBinder_fromJavaBinder(env, binder);
	auto apal = dynamic_cast<aap::AndroidPluginHostPAL*>(aap::getPluginHostPAL());
    apal->serviceConnections.push_back(aap::AudioPluginServiceConnection(serviceIdentifierDup, binderRef, aiBinder));
}

JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_removeBinderForHost(JNIEnv *env, jclass clazz,
                                                                   jstring serviceIdentifier) {
    __android_log_print(ANDROID_LOG_DEBUG, "AAPNativeBridge", "TODO: implement removeBinderFromHost");
}

JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_instantiatePlugin(JNIEnv *env, jclass clazz,
                                                                 jstring serviceIdentifier, jstring pluginId) {
	__android_log_print(ANDROID_LOG_DEBUG, "AAPNativeBridge", "TODO: implement instantiatePlugin");
}

} // extern "C"
