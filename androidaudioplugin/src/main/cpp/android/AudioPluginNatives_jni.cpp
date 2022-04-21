
#include <jni.h>
#include <android/log.h>
#include <android/sharedmem_jni.h>
#include <cstdlib>
#include <sys/mman.h>
#include "aidl/org/androidaudioplugin/BnAudioPluginInterface.h"
#include "aidl/org/androidaudioplugin/BpAudioPluginInterface.h"
#include "aap/core/host/audio-plugin-host.h"
#include "aap/core/host/android/audio-plugin-host-android.h"
#include "aap/core/host/android/android-application-context.h"
#include "AudioPluginInterfaceImpl.h"
#include "audio-plugin-host-android-internal.h"

template <typename T>
T usingJNIEnv(std::function<T(JNIEnv*)> func) {
	JNIEnv *env;
	JavaVM* vm = aap::get_android_jvm();
	assert(vm);
	auto envState = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
	if (envState == JNI_EDETACHED)
		vm->AttachCurrentThread(&env, nullptr);
	assert(env);

	T ret = func(env);

	if (envState == JNI_EDETACHED)
		vm->DetachCurrentThread();
	return ret;
}

template <typename T>
T usingUTFChars(const char* s, std::function<T(jstring)> func) {
    return usingJNIEnv<T>([=](JNIEnv* env) {
        jstring js = env->NewStringUTF(s);
        T ret = func(js);
        return ret;
    });
}

template <typename T>
T usingJString(jstring s, std::function<T(const char*)> func) {
	return usingJNIEnv<T>([=](JNIEnv* env) {
		if (!s)
			return func(nullptr);
		const char *u8 = env->GetStringUTFChars(s, nullptr);
		auto ret = func(u8);
        env->ReleaseStringUTFChars(s, u8);
		return ret;
	});

}

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
		*java_extension_information_class_name = "org/androidaudioplugin/ExtensionInformation",
		*java_port_information_class_name = "org/androidaudioplugin/PortInformation";

static jmethodID
		j_method_is_out_process,
		j_method_get_package_name,
		j_method_get_local_name,
		j_method_get_name,
		j_method_get_manufacturer,
		j_method_get_version,
		j_method_get_plugin_id,
		j_method_get_shared_library_filename,
		j_method_get_library_entrypoint,
		j_method_get_category,
		j_method_get_extension_count,
		j_method_get_extension,
		j_method_extension_get_required,
		j_method_extension_get_uri,
		j_method_get_port_count,
		j_method_get_port,
		j_method_port_get_index,
		j_method_port_get_name,
		j_method_port_get_direction,
		j_method_port_get_content,
		j_method_port_has_value_range,
		j_method_port_get_default,
		j_method_port_get_minimum,
		j_method_port_get_maximum;

void initializeJNIMetadata()
{
    JNIEnv *env;
    auto vm = aap::get_android_jvm();
    assert(vm);
    vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    assert(env);

	jclass java_plugin_information_class = env->FindClass(java_plugin_information_class_name),
			java_extension_information_class = env->FindClass(java_extension_information_class_name),
			java_port_information_class = env->FindClass(java_port_information_class_name);

	j_method_is_out_process = env->GetMethodID(java_plugin_information_class,
											   "isOutProcess", "()Z");
	j_method_get_package_name = env->GetMethodID(java_plugin_information_class, "getPackageName",
													   "()Ljava/lang/String;");
	j_method_get_local_name = env->GetMethodID(java_plugin_information_class, "getLocalName",
												 "()Ljava/lang/String;");
	j_method_get_name = env->GetMethodID(java_plugin_information_class, "getDisplayName",
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
	j_method_get_category = env->GetMethodID(java_plugin_information_class,
													   "getCategory",
													   "()Ljava/lang/String;");
	j_method_get_extension_count = env->GetMethodID(java_plugin_information_class,
											   "getExtensionCount", "()I");
	j_method_get_extension = env->GetMethodID(java_plugin_information_class, "getExtension",
										 "(I)Lorg/androidaudioplugin/ExtensionInformation;");
	j_method_extension_get_required = env->GetMethodID(java_extension_information_class, "getRequired",
											   "()Z");
	j_method_extension_get_uri = env->GetMethodID(java_extension_information_class, "getUri",
											  "()Ljava/lang/String;");
	j_method_get_port_count = env->GetMethodID(java_plugin_information_class,
											   "getPortCount", "()I");
	j_method_get_port = env->GetMethodID(java_plugin_information_class, "getPort",
										 "(I)Lorg/androidaudioplugin/PortInformation;");
	j_method_port_get_index = env->GetMethodID(java_port_information_class, "getIndex",
											  "()I");
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
}

const char* keepPointer(std::vector<const char*> freeList, const char* ptr) {
	freeList.emplace_back(ptr);
	return ptr;
}

aap::PluginInformation *
pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation) {
	std::vector<const char*> freeList{};
	auto aapPI = new aap::PluginInformation(
			env->CallBooleanMethod(pluginInformation, j_method_is_out_process),
			keepPointer(freeList, strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
																 j_method_get_package_name))),
			keepPointer(freeList, strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
																 j_method_get_local_name))),
			keepPointer(freeList, strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
																 j_method_get_name))),
			keepPointer(freeList, strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
																 j_method_get_manufacturer))),
			keepPointer(freeList, strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
																 j_method_get_version))),
			keepPointer(freeList, strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
																 j_method_get_plugin_id))),
			keepPointer(freeList, strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
																 j_method_get_shared_library_filename))),
			keepPointer(freeList, strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
																 j_method_get_library_entrypoint))),
			"", // metadataFullPath, no use on Android
			keepPointer(freeList, strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
																 j_method_get_category)))
	);
	for (auto p : freeList)
		free((void*) p);

	int nExtensions = env->CallIntMethod(pluginInformation, j_method_get_extension_count);
	for (int i = 0; i < nExtensions; i++) {
		jobject port = env->CallObjectMethod(pluginInformation, j_method_get_extension, i);
		auto required = (bool) env->CallBooleanMethod(port, j_method_extension_get_required);
		auto name = strdup_fromJava(env,
									(jstring) env->CallObjectMethod(port, j_method_extension_get_uri));
		aapPI->addExtension(aap::PluginExtensionInformation{required, name});
		free((void*) name);
	}

	int nPorts = env->CallIntMethod(pluginInformation, j_method_get_port_count);
	for (int i = 0; i < nPorts; i++) {
		jobject port = env->CallObjectMethod(pluginInformation, j_method_get_port, i);
		auto index = (uint32_t) env->CallIntMethod(port, j_method_port_get_index);
		auto name = strdup_fromJava(env, (jstring) env->CallObjectMethod(port, j_method_port_get_name));
		auto content = (aap::ContentType) (int) env->CallIntMethod(port, j_method_port_get_content);
		auto direction = (aap::PortDirection) (int) env->CallIntMethod(port, j_method_port_get_direction);
		auto nativePort = new aap::PortInformation(index, name, content, direction);
		if (env->CallBooleanMethod(port, j_method_port_has_value_range)) {
			nativePort->setPropertyValueString(AAP_PORT_DEFAULT, std::to_string(env->CallFloatMethod(port, j_method_port_get_default)));
			nativePort->setPropertyValueString(AAP_PORT_MINIMUM, std::to_string(env->CallFloatMethod(port, j_method_port_get_minimum)));
			nativePort->setPropertyValueString(AAP_PORT_MAXIMUM, std::to_string(env->CallFloatMethod(port, j_method_port_get_maximum)));
		}
		aapPI->addPort(nativePort);
		free((void*) name);
	}

	return aapPI;
}


// --------------------------------------------------

jobjectArray queryInstalledPluginsJNI()
{
	return usingJNIEnv<jobjectArray> ([](JNIEnv *env) {
		jclass java_audio_plugin_host_helper_class = env->FindClass(
				"org/androidaudioplugin/hosting/AudioPluginHostHelper");
		assert(java_audio_plugin_host_helper_class);
		jmethodID j_method_query_audio_plugins = env->GetStaticMethodID(
				java_audio_plugin_host_helper_class, "queryAudioPlugins",
				"(Landroid/content/Context;)[Lorg/androidaudioplugin/PluginInformation;");
		assert(j_method_query_audio_plugins);
		return (jobjectArray) env->CallStaticObjectMethod(java_audio_plugin_host_helper_class,
															  j_method_query_audio_plugins,
															  aap::get_android_application_context());
	});
}

// --------------------------------------------------

JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_initializeAAPJni(JNIEnv *env, jclass clazz,
                                                                jobject applicationContext) {
    aap::set_application_context(env, applicationContext);
    initializeJNIMetadata();
}

// --------------------------------------------------

std::shared_ptr<aidl::org::androidaudioplugin::BnAudioPluginInterface> sp_binder;

jobject
Java_org_androidaudioplugin_AudioPluginNatives_createBinderForService(JNIEnv *env, jclass clazz) {
    sp_binder = ndk::SharedRefBase::make<aap::AudioPluginInterfaceImpl>();
    auto ret = AIBinder_toJavaBinder(env, sp_binder->asBinder().get());
    return ret;
}

void
Java_org_androidaudioplugin_AudioPluginNatives_destroyBinderForService(JNIEnv *env, jclass clazz,
                                                                      jobject binder) {
    auto abinder = AIBinder_fromJavaBinder(env, binder);
    AIBinder_decStrong(abinder);
    sp_binder.reset();
}

// --------------------------------------------------

std::map<jint, aap::PluginClientConnectionList*> client_connection_list_per_scope{};

int32_t getConnectorInstanceId(aap::PluginClientConnectionList* connections) {
    for (auto entry : client_connection_list_per_scope)
		if (entry.second == connections)
			return entry.first;
	return -1;
}

aap::PluginClientConnectionList* getPluginConnectionListFromJni(jint connectorInstanceId, bool createIfNotExist) {
	if (client_connection_list_per_scope.find(connectorInstanceId) != client_connection_list_per_scope.end())
		return client_connection_list_per_scope[connectorInstanceId];
	if (!createIfNotExist)
		return nullptr;
	auto ret = new aap::PluginClientConnectionList();
	client_connection_list_per_scope[connectorInstanceId] = ret;
	return ret;
}

JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_addBinderForClient(JNIEnv *env, jclass clazz, jint connectorInstanceId,
                                                                jstring packageName, jstring className, jobject binder) {
	usingJString<void*>(packageName, [=](const char* packageNameChars) {
		usingJString<void*>(className, [=](const char* classNameChars) {
			auto aiBinder = AIBinder_fromJavaBinder(env, binder);

			auto list = client_connection_list_per_scope[connectorInstanceId];
			if (list == nullptr) {
				client_connection_list_per_scope[connectorInstanceId] = new aap::PluginClientConnectionList();
				list = client_connection_list_per_scope[connectorInstanceId];
			}
			list->add(std::make_unique<aap::PluginClientConnection>(packageNameChars, classNameChars, aiBinder));
			return nullptr;
		});
		return nullptr;
	});
}

JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_removeBinderForHost(JNIEnv *env, jclass clazz, jint connectorInstanceId,
																   jstring packageName, jstring className) {
	usingJString<void*>(packageName, [=](const char* packageNameChars) {
		usingJString<void*>(className, [=](const char* classNameChars) {
			auto list = client_connection_list_per_scope[connectorInstanceId];
			if (list != nullptr)
				list->remove(packageNameChars, classNameChars);
			return nullptr;
		});
		return nullptr;
	});
}

// --------------------------------------------------

jobject audio_plugin_service_connector{nullptr};
std::map<std::string,std::function<void(std::string)> > inProgressCallbacks{};

void ensureServiceConnectedFromJni(jint connectorInstanceId, std::string servicePackageName, std::function<void(std::string)> callback) {
	inProgressCallbacks[servicePackageName] = callback;

	usingJNIEnv<void*> ([=](JNIEnv *env) {

        if (audio_plugin_service_connector == nullptr) {
            jclass connector_class = env->FindClass("org/androidaudioplugin/hosting/AudioPluginServiceConnector");
            jmethodID constructor = env->GetMethodID(connector_class, "<init>", "(Landroid/content/Context;)V");
            audio_plugin_service_connector = env->NewGlobalRef(env->NewObject(connector_class, constructor, aap::get_android_application_context()));
        }

		jclass java_audio_plugin_host_helper_class = env->FindClass(
				"org/androidaudioplugin/hosting/AudioPluginHostHelper");
		assert(java_audio_plugin_host_helper_class);
		jmethodID j_method_ensure_instance_created = env->GetStaticMethodID(
				java_audio_plugin_host_helper_class, "ensureBinderConnected",
				"(Ljava/lang/String;Lorg/androidaudioplugin/hosting/AudioPluginServiceConnector;)V");
		assert(j_method_ensure_instance_created);

        return usingUTFChars<void *>(servicePackageName.c_str(), [=](jstring packageName) {
			env->CallStaticVoidMethod(java_audio_plugin_host_helper_class,
									  j_method_ensure_instance_created,
									  packageName,
									  audio_plugin_service_connector);

			return nullptr;
		});
	});
}

JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_AudioPluginServiceConnector_nativeOnServiceConnectedCallback(
		JNIEnv *env, jobject thiz, jstring servicePackageName) {
	jboolean wasCopy;
	const char* s = env->GetStringUTFChars(servicePackageName, &wasCopy);
	auto entry = inProgressCallbacks.find(s);
	if (entry != inProgressCallbacks.end())
		// FIXME: what kind of error propagation could be achieved here?
		entry->second("");
    env->ReleaseStringUTFChars(servicePackageName, s);
}

// --------------------------------------------------

JNIEXPORT int JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_getSharedMemoryFD(JNIEnv *env, jclass clazz, jobject shm) {
	return ASharedMemory_dupFromJava(env, shm);
}

JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_closeSharedMemoryFD(JNIEnv *env, jclass clazz, int fd) {
	close(fd);
}
} // extern "C"
