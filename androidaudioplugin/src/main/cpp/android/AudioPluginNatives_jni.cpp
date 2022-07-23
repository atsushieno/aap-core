
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
#include "AudioPluginNative_jni.h"


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
		j_method_get_declared_port_count,
		j_method_get_declared_port,
		j_method_port_ctor,
		j_method_port_get_index,
		j_method_port_get_name,
		j_method_port_get_direction,
		j_method_port_get_content,
		j_method_port_has_value_range,
		j_method_port_get_default,
		j_method_port_get_minimum,
		j_method_port_get_maximum,
		j_method_port_get_minimum_size_in_bytes;

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
	j_method_get_declared_port_count = env->GetMethodID(java_plugin_information_class,
														"getDeclaredPortCount", "()I");
	j_method_get_declared_port = env->GetMethodID(java_plugin_information_class, "getDeclaredPort",
										 "(I)Lorg/androidaudioplugin/PortInformation;");
	j_method_port_ctor = env->GetMethodID(java_port_information_class, "<init>",
                                          "(ILjava/lang/String;IIFFF)V");
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
	j_method_port_get_minimum_size_in_bytes = env->GetMethodID(java_port_information_class, "getMinimumSizeInBytes",
												 "()I");
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

	int nPorts = env->CallIntMethod(pluginInformation, j_method_get_declared_port_count);
	for (int i = 0; i < nPorts; i++) {
		jobject port = env->CallObjectMethod(pluginInformation, j_method_get_declared_port, i);
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
		nativePort->setPropertyValueString(AAP_PORT_MINIMUM_SIZE, std::to_string(env->CallIntMethod(port, j_method_port_get_minimum_size_in_bytes)));
		aapPI->addDeclaredPort(nativePort);
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

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_initializeAAPJni(JNIEnv *env, jclass clazz,
                                                                jobject applicationContext) {
    aap::set_application_context(env, applicationContext);
    initializeJNIMetadata();
}

// --------------------------------------------------

std::shared_ptr<aidl::org::androidaudioplugin::BnAudioPluginInterface> sp_binder;

extern "C"
JNIEXPORT jobject JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_createBinderForService(JNIEnv *env, jclass clazz) {
    sp_binder = ndk::SharedRefBase::make<aap::AudioPluginInterfaceImpl>();
    auto ret = AIBinder_toJavaBinder(env, sp_binder->asBinder().get());
    return ret;
}

extern "C"
JNIEXPORT void JNICALL
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

extern "C"
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

extern "C"
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

extern "C"
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

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_getSharedMemoryFD(JNIEnv *env, jclass clazz, jobject shm) {
	return ASharedMemory_dupFromJava(env, shm);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_closeSharedMemoryFD(JNIEnv *env, jclass clazz, int fd) {
	close(fd);
}



AndroidAudioPlugin *JNIClientAAPXSManager::getPlugin() {
    return nullptr;
}

AAPXSFeature *JNIClientAAPXSManager::getExtensionFeature(const char *uri) {
	return nullptr;
}

const aap::PluginInformation *JNIClientAAPXSManager::getPluginInformation() {
	return nullptr;
}

AAPXSClientInstance *JNIClientAAPXSManager::setupAAPXSInstance(AAPXSFeature *feature, int32_t dataSize) {
	return nullptr;
}

aap::PluginListSnapshot cached_plugin_list{};


extern "C"
JNIEXPORT jlong JNICALL
Java_org_androidaudioplugin_hosting_NativePluginClient_newInstance(JNIEnv *env, jclass clazz,
																   jint connectorInstanceId) {
	auto connections = getPluginConnectionListFromJni(connectorInstanceId, true);
	assert(connections);
	if (cached_plugin_list.getNumPluginInformation() == 0)
		cached_plugin_list = aap::PluginListSnapshot::queryServices();
	return (jlong) (void*) new aap::PluginClient(connections, &cached_plugin_list);
}

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_createRemotePluginInstance(
		JNIEnv *env, jclass clazz, jstring plugin_id, jint sampleRate, jlong clientPointer) {
	assert(clientPointer != 0);
	auto client = (aap::PluginClient*) (void*) clientPointer;
	return usingJString<jlong>(plugin_id, [&](const char* pluginId) {
		auto result = client->createInstance(pluginId, sampleRate, true);
		if (!result.error.empty()) {
            aap::a_log(AAP_LOG_LEVEL_ERROR, "AAP", result.error.c_str());
            return (jlong) -1;
        }
        auto instance = client->getInstance(result.value);
        instance->completeInstantiation();
        return (jlong) result.value;
	});
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_prepare(JNIEnv *env, jclass clazz,
																			jlong nativeClient,
																			jint instanceId,
																			jint frameCount,
																			jint defaultControlBytesPerBlock) {
	auto client = (aap::PluginClient*) (void*) nativeClient;
	auto instance = client->getInstance(instanceId);
    auto shm = instance->getAAPXSSharedMemoryStore();
    auto numPorts = instance->getNumPorts();
    shm->resizePortBufferByCount(numPorts);
	auto buffer = instance->getAudioPluginBuffer(numPorts, frameCount, defaultControlBytesPerBlock);
	instance->prepare(frameCount, buffer);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_activate(JNIEnv *env, jclass clazz,
																		jlong nativeClient,
																		jint instanceId) {
    auto client = (aap::PluginClient*) (void*) nativeClient;
    auto instance = client->getInstance(instanceId);
	instance->activate();
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_process(JNIEnv *env, jclass clazz,
																	   jlong nativeClient,
																	   jint instanceId,
																	   jint timeout_in_nanoseconds) {
    auto client = (aap::PluginClient*) (void*) nativeClient;
    auto instance = client->getInstance(instanceId);
	auto buffer = (AndroidAudioPluginBuffer*) instance->getAudioPluginBuffer();
	instance->process(buffer, timeout_in_nanoseconds);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_deactivate(JNIEnv *env, jclass clazz,
																		  jlong nativeClient,
																		  jint instanceId) {
    auto client = (aap::PluginClient*) (void*) nativeClient;
    auto instance = client->getInstance(instanceId);
	instance->deactivate();
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_destroy(JNIEnv *env, jclass clazz,
																	   jlong nativeClient,
																	   jint instanceId) {
    auto client = (aap::PluginClient*) (void*) nativeClient;
    auto instance = client->getInstance(instanceId);
    aap::a_log(AAP_LOG_LEVEL_INFO, "AAP_DEBUG", "!!!!!!! deleting instance !!!!!!!");
    client->destroyInstance(instance);
    aap::a_log(AAP_LOG_LEVEL_INFO, "AAP_DEBUG", "!!!!!!! deleted instance !!!!!!!");
}

// State extensions

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getStateSize(JNIEnv *env,
																			jclass clazz,
																			jlong nativeClient,
																			jint instanceId) {
    auto client = (aap::PluginClient*) (void*) nativeClient;
    auto instance = client->getInstance(instanceId);
	return instance->getStandardExtensions().getStateSize();
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getState(JNIEnv *env, jclass clazz,
																		jlong nativeClient,
																		jint instanceId,
																		jbyteArray data) {
    auto client = (aap::PluginClient*) (void*) nativeClient;
    auto instance = client->getInstance(instanceId);
	auto state = instance->getStandardExtensions().getState();
	env->SetByteArrayRegion(data, 0, state.data_size, static_cast<const jbyte *>(state.data));
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_setState(JNIEnv *env, jclass clazz,
																		jlong nativeClient,
																		jint instanceId,
																		jbyteArray data) {
    auto client = (aap::PluginClient*) (void*) nativeClient;
    auto instance = client->getInstance(instanceId);
	auto length = env->GetArrayLength(data);
	jboolean wasCopy;
	auto buf = env->GetByteArrayElements(data, &wasCopy);
	instance->getStandardExtensions().setState(buf, length);
	if (wasCopy)
		free(buf);
}

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getPortBufferFD(JNIEnv *env,
																			 jclass clazz,
																			 jlong nativeClient,
																			 jint instanceId,
																			 jint index) {
	auto client = (aap::PluginClient *) (void *) nativeClient;
	auto instance = client->getInstance(instanceId);
	return (jint) instance->getAAPXSSharedMemoryStore()->getPortBufferFD(index);
}

// Presets extensions

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getPresetCount(JNIEnv *env,
																			  jclass clazz,
																			  jlong nativeClient,
																			  jint instanceId) {
	auto client = (aap::PluginClient*) (void*) nativeClient;
	auto instance = client->getInstance(instanceId);
	return instance->getStandardExtensions().getPresetCount();
}

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getCurrentPresetIndex(JNIEnv *env,
																					 jclass clazz,
																					 jlong nativeClient,
																					 jint instanceId) {
	auto client = (aap::PluginClient*) (void*) nativeClient;
	auto instance = client->getInstance(instanceId);
	return instance->getStandardExtensions().getCurrentPresetIndex();
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_setCurrentPresetIndex(JNIEnv *env,
																					 jclass clazz,
																					 jlong nativeClient,
																					 jint instanceId,
																					 jint index) {
	auto client = (aap::PluginClient*) (void*) nativeClient;
	auto instance = client->getInstance(instanceId);
	instance->getStandardExtensions().setCurrentPresetIndex(index);
}

extern "C"
JNIEXPORT jstring JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getCurrentPresetName(JNIEnv *env,
																					jclass clazz,
																					jlong nativeClient,
																					jint instanceId,
																					jint index) {
	auto client = (aap::PluginClient*) (void*) nativeClient;
	auto instance = client->getInstance(instanceId);
	return env->NewStringUTF(instance->getStandardExtensions().getCurrentPresetName(index).c_str());
}

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getPortCount(JNIEnv *env,
                                                                            jclass clazz,
                                                                            jlong nativeClient,
                                                                            jint instanceId) {
	auto client = (aap::PluginClient*) (void*) nativeClient;
	auto instance = client->getInstance(instanceId);
	return instance->getNumPorts();
}

extern "C"
JNIEXPORT jobject JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getPort(JNIEnv *env, jclass clazz,
																	   jlong nativeClient,
																	   jint instanceId,
																	   jint index) {
	auto client = (aap::PluginClient*) (void*) nativeClient;
	auto instance = client->getInstance(instanceId);
	auto port = instance->getPort(index);
	auto klass = env->FindClass(java_port_information_class_name);
	assert(klass);
	return env->NewObject(klass, j_method_port_ctor, (jint) port->getIndex(), env->NewStringUTF(port->getName()), (jint) port->getPortDirection(), (jint) port->getContentType(), port->getDefaultValue(), port->getMinimumValue(), port->getMaximumValue());
}
extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getPortBuffer(JNIEnv *env,
		jclass clazz,
		jlong nativeClient,
		jint instanceId,
		jint portIndex,
        // Note that you will have to pass native buffer (using ByteBuffer.allocateDirecT()), otherwise ART crashes.
		jobject buffer,
		jint size) {
	auto client = (aap::PluginClient*) (void*) nativeClient;
	auto instance = client->getInstance(instanceId);
	auto src = instance->getAudioPluginBuffer()->buffers[portIndex];
    auto dst = env->GetDirectBufferAddress(buffer);
	memcpy(dst, src, size);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_setPortBuffer(JNIEnv *env,
		jclass clazz,
		jlong nativeClient,
		jint instanceId,
		jint portIndex,
        // Note that you will have to pass native buffer (using ByteBuffer.allocateDirecT()), otherwise ART crashes.
		jobject buffer,
		jint size) {
	auto client = (aap::PluginClient *) (void *) nativeClient;
	auto instance = client->getInstance(instanceId);
    auto src = env->GetDirectBufferAddress(buffer);
	auto dst = instance->getAudioPluginBuffer()->buffers[portIndex];
	memcpy(dst, src, size);
}
