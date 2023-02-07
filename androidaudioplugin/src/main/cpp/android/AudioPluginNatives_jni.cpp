
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

std::string jstringToStdString(JNIEnv* env, jstring s) {
    jboolean b{false};
    const char *u8 = env->GetStringUTFChars(s, &b);
    std::string ret{u8};
    if (b)
        env->ReleaseStringUTFChars(s, u8);
    return ret;
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
		*java_parameter_information_class_name = "org/androidaudioplugin/ParameterInformation",
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
        j_method_get_declared_parameter_count,
        j_method_get_declared_parameter,
		j_method_get_declared_port_count,
		j_method_get_declared_port,
		j_method_parameter_ctor,
        j_method_parameter_get_id,
        j_method_parameter_get_name,
        j_method_parameter_get_default_value,
        j_method_parameter_get_minimum_value,
        j_method_parameter_get_maximum_value,
		j_method_port_ctor,
		j_method_port_get_index,
		j_method_port_get_name,
		j_method_port_get_direction,
		j_method_port_get_content,
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
			java_parameter_information_class = env->FindClass(java_parameter_information_class_name),
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
    j_method_get_declared_parameter_count = env->GetMethodID(java_plugin_information_class,
                                                        "getDeclaredParameterCount", "()I");
    j_method_get_declared_parameter = env->GetMethodID(java_plugin_information_class, "getDeclaredParameter",
                                                  "(I)Lorg/androidaudioplugin/ParameterInformation;");
	j_method_get_declared_port_count = env->GetMethodID(java_plugin_information_class,
														"getDeclaredPortCount", "()I");
	j_method_get_declared_port = env->GetMethodID(java_plugin_information_class, "getDeclaredPort",
										 "(I)Lorg/androidaudioplugin/PortInformation;");
	j_method_parameter_ctor = env->GetMethodID(java_parameter_information_class, "<init>",
										  "(ILjava/lang/String;DDD)V");
    j_method_parameter_get_id = env->GetMethodID(java_parameter_information_class, "getId",
                                                 "()I");
    j_method_parameter_get_name = env->GetMethodID(java_parameter_information_class, "getName",
                                                   "()Ljava/lang/String;");
    j_method_parameter_get_default_value = env->GetMethodID(java_parameter_information_class, "getDefaultValue",
                                                            "()D");
    j_method_parameter_get_minimum_value = env->GetMethodID(java_parameter_information_class, "getMinimumValue",
                                                            "()D");
    j_method_parameter_get_maximum_value = env->GetMethodID(java_parameter_information_class, "getMaximumValue",
                                                            "()D");
	j_method_port_ctor = env->GetMethodID(java_port_information_class, "<init>",
                                          "(ILjava/lang/String;II)V");
	j_method_port_get_index = env->GetMethodID(java_port_information_class, "getIndex",
											  "()I");
	j_method_port_get_name = env->GetMethodID(java_port_information_class, "getName",
											  "()Ljava/lang/String;");
	j_method_port_get_direction = env->GetMethodID(java_port_information_class,
												   "getDirection", "()I");
	j_method_port_get_content = env->GetMethodID(java_port_information_class, "getContent",
												 "()I");
	j_method_port_get_minimum_size_in_bytes = env->GetMethodID(java_port_information_class, "getMinimumSizeInBytes",
												 "()I");
}

#define CLEAR_JNI_ERROR(env) { if (env->ExceptionOccurred()) { env->ExceptionDescribe(); env->ExceptionClear(); } }

// It is required where the current Dalvik thread cannot resolve classes from current Context (app)...
// JNI_OnLoad() at AudioPluginService does not seem to work enough to resolve classes in the apk, unlike
// explained at https://developer.android.com/training/articles/perf-jni#faq:-why-didnt-findclass-find-my-class
extern "C" jclass getAudioPluginMidiSettingsClass() {
    JNIEnv *env;
    aap::get_android_jvm()->GetEnv((void**) &env, JNI_VERSION_1_6);

    auto context = aap::get_android_application_context();
    auto contextClass = env->FindClass("android/content/Context");
    auto getClassLoader = env->GetMethodID(contextClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    auto contextClassLoader = env->CallObjectMethod(context, getClassLoader);

    auto classLoaderClass = env->FindClass("java/lang/ClassLoader");
    CLEAR_JNI_ERROR(env)
    assert(classLoaderClass);
    auto findClassMethod = env->GetMethodID(classLoaderClass, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    CLEAR_JNI_ERROR(env)
    assert(findClassMethod);
    auto settingsClassObj = env->CallObjectMethod(contextClassLoader, findClassMethod,
                                                  env->NewStringUTF("org/androidaudioplugin/hosting/AudioPluginMidiSettings"));
    CLEAR_JNI_ERROR(env)
    assert(settingsClassObj);
    return (jclass) settingsClassObj;
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

    int nParameters = env->CallIntMethod(pluginInformation, j_method_get_declared_parameter_count);
    for (int i = 0; i < nParameters; i++) {
        jobject para = env->CallObjectMethod(pluginInformation, j_method_get_declared_parameter, i);
        auto id = (uint32_t) env->CallIntMethod(para, j_method_parameter_get_id);
        auto name = strdup_fromJava(env, (jstring) env->CallObjectMethod(para, j_method_parameter_get_name));
        auto def = env->CallDoubleMethod(para, j_method_parameter_get_default_value);
        auto min = env->CallDoubleMethod(para, j_method_parameter_get_minimum_value);
        auto max = env->CallDoubleMethod(para, j_method_parameter_get_maximum_value);
        auto nativePara = new aap::ParameterInformation(id, name, min, max, def);
        aapPI->addDeclaredParameter(nativePara);
        free((void*) name);
    }

	int nPorts = env->CallIntMethod(pluginInformation, j_method_get_declared_port_count);
	for (int i = 0; i < nPorts; i++) {
		jobject port = env->CallObjectMethod(pluginInformation, j_method_get_declared_port, i);
		auto index = (uint32_t) env->CallIntMethod(port, j_method_port_get_index);
		auto name = strdup_fromJava(env, (jstring) env->CallObjectMethod(port, j_method_port_get_name));
		auto content = (aap_content_type) (int) env->CallIntMethod(port, j_method_port_get_content);
		auto direction = (aap_port_direction) (int) env->CallIntMethod(port, j_method_port_get_direction);
		auto nativePort = new aap::PortInformation(index, name, content, direction);
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
int32_t getMidiSettingsFromLocalConfig(std::string pluginId) {
	return usingJNIEnv<int32_t> ([pluginId](JNIEnv *env) {
		auto java_audio_plugin_midi_settings_class = getAudioPluginMidiSettingsClass();
		assert(java_audio_plugin_midi_settings_class);
		jmethodID j_method_get_midi_settings_from_shared_preference = env->GetStaticMethodID(
				java_audio_plugin_midi_settings_class, "getMidiSettingsFromSharedPreference",
				"(Landroid/content/Context;Ljava/lang/String;)I");
		assert(j_method_get_midi_settings_from_shared_preference);
		auto context = aap::get_android_application_context();
		auto pluginIdJString = env->NewStringUTF(pluginId.c_str());
		return env->CallStaticIntMethod(java_audio_plugin_midi_settings_class, j_method_get_midi_settings_from_shared_preference, context, pluginIdJString);
	});
}

extern "C"
void putMidiSettingsToSharedPreference(std::string pluginId, int32_t flags) {
	usingJNIEnv<int32_t> ([pluginId,flags](JNIEnv *env) {
		auto java_audio_plugin_midi_settings_class = getAudioPluginMidiSettingsClass();
		assert(java_audio_plugin_midi_settings_class);
		jmethodID j_method_put_midi_settings_to_shared_preference = env->GetStaticMethodID(
				java_audio_plugin_midi_settings_class, "putMidiSettingsToSharedPreference",
				"(Landroid/content/Context;Ljava/lang/String;I)V");
		assert(j_method_put_midi_settings_to_shared_preference);
		auto context = aap::get_android_application_context();
		auto pluginIdJString = env->NewStringUTF(pluginId.c_str());
		env->CallStaticVoidMethod(java_audio_plugin_midi_settings_class, j_method_put_midi_settings_to_shared_preference, context, pluginIdJString, flags);
		return 0;
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

std::map<AIBinder*,aap::AndroidPluginClientConnectionData*> live_connection_data{};

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_addBinderForClient(JNIEnv *env, jclass clazz, jint connectorInstanceId,
                                                                jstring packageName, jstring className, jobject binder) {
    std::string packageNameString = jstringToStdString(env, packageName);
    std::string classNameString = jstringToStdString(env, className);
	auto aiBinder = AIBinder_fromJavaBinder(env, binder);
	aap::AndroidPluginClientConnectionData* connectionData = nullptr;
	for (auto &pair : live_connection_data)
		if (pair.first == aiBinder)
			connectionData = pair.second;
    if (!connectionData) {
        connectionData = new aap::AndroidPluginClientConnectionData(aiBinder);
        live_connection_data[aiBinder] = connectionData;
		AIBinder_incStrong(aiBinder);
    }
	AIBinder_decStrong(aiBinder);

    auto list = client_connection_list_per_scope[connectorInstanceId];
    if (list == nullptr) {
        client_connection_list_per_scope[connectorInstanceId] = new aap::PluginClientConnectionList();
        list = client_connection_list_per_scope[connectorInstanceId];
    }
    list->add(std::make_unique<aap::PluginClientConnection>(packageNameString, classNameString, connectionData));
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_removeBinderForClient(JNIEnv *env, jclass clazz, jint connectorInstanceId,
																   jstring packageName, jstring className) {
	std::string packageNameString = jstringToStdString(env, packageName);
	std::string classNameString = jstringToStdString(env, className);
	auto list = client_connection_list_per_scope[connectorInstanceId];
	if (list != nullptr)
		list->remove(packageNameString.c_str(), classNameString.c_str());
}

// --------------------------------------------------

jobject audio_plugin_service_connector{nullptr};
std::map<std::string,std::function<void(std::string&)> > inProgressCallbacks{};

void ensureServiceConnectedFromJni(jint connectorInstanceId, std::string servicePackageName, std::function<void(std::string&)> callback) {
	inProgressCallbacks[servicePackageName] = callback;

	usingJNIEnv<void*> ([&](JNIEnv *env) {

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

		env->CallStaticVoidMethod(java_audio_plugin_host_helper_class,
									  j_method_ensure_instance_created,
									  env->NewStringUTF(servicePackageName.c_str()),
									  audio_plugin_service_connector);
		return nullptr;
	});
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_AudioPluginServiceConnector_nativeOnServiceConnectedCallback(
		JNIEnv *env, jobject thiz, jstring servicePackageName) {
	jboolean wasCopy;
	const char* s = env->GetStringUTFChars(servicePackageName, &wasCopy);
	auto entry = inProgressCallbacks.find(s);
	if (entry != inProgressCallbacks.end()) {
		// FIXME: what kind of error propagation could be achieved here?
		std::string empty{};
		entry->second(empty);
	}
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
	std::string pluginIdString = jstringToStdString(env, plugin_id);
	auto result = client->createInstance(pluginIdString.c_str(), sampleRate, true);
	if (!result.error.empty()) {
		aap::a_log(AAP_LOG_LEVEL_ERROR, "AAP", result.error.c_str());
		return (jlong) -1;
	}
	auto instance = dynamic_cast<aap::RemotePluginInstance*>(client->getInstanceById(result.value));
	assert(instance);
	instance->configurePorts();
	return (jlong) result.value;
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_prepare(JNIEnv *env, jclass clazz,
																			jlong nativeClient,
																			jint instanceId,
																			jint frameCount,
																			jint defaultControlBytesPerBlock) {
	auto client = (aap::PluginClient*) (void*) nativeClient;
	auto instance = client->getInstanceById(instanceId);
    auto numPorts = instance->getNumPorts();
	instance->allocateAudioPluginBuffer(numPorts, frameCount, defaultControlBytesPerBlock);
	instance->prepare(frameCount, instance->getPluginBuffer()->toPublicApi());
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_activate(JNIEnv *env, jclass clazz,
																		jlong nativeClient,
																		jint instanceId) {
    auto client = (aap::PluginClient*) (void*) nativeClient;
    auto instance = client->getInstanceById(instanceId);
	instance->activate();
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_process(JNIEnv *env, jclass clazz,
																	   jlong nativeClient,
																	   jint instanceId,
																	   jint timeout_in_nanoseconds) {
    auto client = (aap::PluginClient*) (void*) nativeClient;
    auto instance = client->getInstanceById(instanceId);
	auto buffer = (AndroidAudioPluginBuffer*) instance->getPluginBuffer()->toPublicApi();
	instance->process(buffer, timeout_in_nanoseconds);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_deactivate(JNIEnv *env, jclass clazz,
																		  jlong nativeClient,
																		  jint instanceId) {
    auto client = (aap::PluginClient*) (void*) nativeClient;
    auto instance = client->getInstanceById(instanceId);
	instance->deactivate();
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_destroy(JNIEnv *env, jclass clazz,
																	   jlong nativeClient,
																	   jint instanceId) {
    auto client = (aap::PluginClient*) (void*) nativeClient;
    auto instance = client->getInstanceById(instanceId);
    client->destroyInstance(instance);
}

// State extensions

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getStateSize(JNIEnv *env,
																			jclass clazz,
																			jlong nativeClient,
																			jint instanceId) {
    auto client = (aap::PluginClient*) (void*) nativeClient;
    auto instance = client->getInstanceById(instanceId);
	return instance->getStandardExtensions().getStateSize();
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getState(JNIEnv *env, jclass clazz,
																		jlong nativeClient,
																		jint instanceId,
																		jbyteArray data) {
    auto client = (aap::PluginClient*) (void*) nativeClient;
    auto instance = client->getInstanceById(instanceId);
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
    auto instance = client->getInstanceById(instanceId);
	auto length = env->GetArrayLength(data);
	jboolean wasCopy;
	auto buf = env->GetByteArrayElements(data, &wasCopy);
	instance->getStandardExtensions().setState(buf, length);
	if (wasCopy)
		free(buf);
}

// Presets extensions

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getPresetCount(JNIEnv *env,
																			  jclass clazz,
																			  jlong nativeClient,
																			  jint instanceId) {
	auto client = (aap::PluginClient*) (void*) nativeClient;
	auto instance = client->getInstanceById(instanceId);
	return instance->getStandardExtensions().getPresetCount();
}

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getCurrentPresetIndex(JNIEnv *env,
																					 jclass clazz,
																					 jlong nativeClient,
																					 jint instanceId) {
	auto client = (aap::PluginClient*) (void*) nativeClient;
	auto instance = client->getInstanceById(instanceId);
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
	auto instance = client->getInstanceById(instanceId);
	instance->getStandardExtensions().setCurrentPresetIndex(index);
}

extern "C"
JNIEXPORT jstring JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getPresetName__JII(JNIEnv *env,
																				  jclass clazz,
																				  jlong nativeClient,
																				  jint instanceId,
																				  jint index) {
	auto client = (aap::PluginClient*) (void*) nativeClient;
	auto instance = client->getInstanceById(instanceId);
	return env->NewStringUTF(instance->getStandardExtensions().getPresetName(index).c_str());
}

// MIDI extension

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getMidiMappingPolicy(JNIEnv *env,
																					jclass clazz,
																					jlong nativeClient,
																					jint instanceId) {
	auto client = (aap::PluginClient *) (void *) nativeClient;
	auto instance = client->getInstanceById(instanceId);
	return (jint) instance->getStandardExtensions().getMidiMappingPolicy(instance->getPluginInformation()->getPluginID());
}

// GUI extension

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_showGui(JNIEnv *env,
																	   jclass clazz,
																	   jlong nativeClient,
																	   jint instanceId) {
	auto client = (aap::PluginClient *) (void *) nativeClient;
	auto instance = client->getInstanceById(instanceId);
	instance->getStandardExtensions().showGui();
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_hideGui(JNIEnv *env,
																	   jclass clazz,
																	   jlong nativeClient,
																	   jint instanceId) {
	auto client = (aap::PluginClient *) (void *) nativeClient;
	auto instance = client->getInstanceById(instanceId);
	instance->getStandardExtensions().hideGui();
}


// plugin instance (dynamic) information retrieval

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getParameterCount(JNIEnv *env,
																			jclass clazz,
																			jlong nativeClient,
																			jint instanceId) {
	auto client = (aap::PluginClient*) (void*) nativeClient;
	auto instance = client->getInstanceById(instanceId);
	return instance->getNumParameters();
}

extern "C"
JNIEXPORT jobject JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getParameter(JNIEnv *env, jclass clazz,
																	   jlong nativeClient,
																	   jint instanceId,
																	   jint index) {
	auto client = (aap::PluginClient*) (void*) nativeClient;
	auto instance = client->getInstanceById(instanceId);
	auto para = instance->getParameter(index);
	auto klass = env->FindClass(java_parameter_information_class_name);
	assert(klass);
	return env->NewObject(klass, j_method_parameter_ctor, (jint) para->getId(), env->NewStringUTF(para->getName()), para->getMinimumValue(), para->getMaximumValue(), para->getDefaultValue());
}

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getPortCount(JNIEnv *env,
                                                                            jclass clazz,
                                                                            jlong nativeClient,
                                                                            jint instanceId) {
	auto client = (aap::PluginClient*) (void*) nativeClient;
	auto instance = client->getInstanceById(instanceId);
	return instance->getNumPorts();
}

extern "C"
JNIEXPORT jobject JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getPort(JNIEnv *env, jclass clazz,
																	   jlong nativeClient,
																	   jint instanceId,
																	   jint index) {
	auto client = (aap::PluginClient*) (void*) nativeClient;
	auto instance = client->getInstanceById(instanceId);
	auto port = instance->getPort(index);
	auto klass = env->FindClass(java_port_information_class_name);
	assert(klass);
	return env->NewObject(klass, j_method_port_ctor, (jint) port->getIndex(), env->NewStringUTF(port->getName()), (jint) port->getPortDirection(), (jint) port->getContentType());
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
	auto instance = client->getInstanceById(instanceId);
	auto src = instance->getPluginBuffer()->getBuffer(portIndex);
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
	auto instance = client->getInstanceById(instanceId);
    auto src = env->GetDirectBufferAddress(buffer);
	auto dst = instance->getPluginBuffer()->getBuffer(portIndex);
	memcpy(dst, src, size);
}
