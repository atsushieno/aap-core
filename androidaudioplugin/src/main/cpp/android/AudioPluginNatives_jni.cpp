
#include <jni.h>
#include <android/log.h>
#include <android/sharedmem_jni.h>
#include <cstdlib>
#include <sys/mman.h>
#include "aidl/org/androidaudioplugin/BnAudioPluginInterface.h"
#include "aidl/org/androidaudioplugin/BpAudioPluginInterface.h"
#include "aap/core/host/audio-plugin-host.h"
#include "aap/core/host/android/audio-plugin-host-android.h"
#include "aap/core/android/android-application-context.h"
#include "AudioPluginInterfaceImpl.h"
#include "audio-plugin-host-android-internal.h"
#include "../core/hosting/plugin-service-list.h"
#include "../core/AAPJniFacade.h"

std::string jstringToStdString(JNIEnv* env, jstring s) {
    jboolean b{false};
    const char *u8 = env->GetStringUTFChars(s, &b);
    std::string ret{u8};
    if (b)
        env->ReleaseStringUTFChars(s, u8);
    return ret;
}


// --------------------------------------------------

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_initializeAAPJni(JNIEnv *env, jclass clazz,
                                                                jobject applicationContext) {
    aap::set_application_context(env, applicationContext);
    aap::AAPJniFacade::getInstance()->initializeJNIMetadata();
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

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_prepareNativeLooper(JNIEnv *env, jclass clazz) {
	aap::prepare_non_rt_event_looper();
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_startNativeLooper(JNIEnv *env, jclass clazz) {
	aap::start_non_rt_event_looper();
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_stopNativeLooper(JNIEnv *env, jclass clazz) {
	aap::stop_non_rt_event_looper();
}

// --------------------------------------------------

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

    aap::AAPJniFacade::getInstance()->addScopedClientConnection(connectorInstanceId,
																packageNameString, classNameString, connectionData);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_AudioPluginNatives_removeBinderForClient(JNIEnv *env, jclass clazz, jint connectorInstanceId,
																   jstring packageName, jstring className) {
	std::string packageNameString = jstringToStdString(env, packageName);
	std::string classNameString = jstringToStdString(env, className);
	aap::AAPJniFacade::getInstance()->removeScopedClientConnection(connectorInstanceId, packageNameString, classNameString);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_AudioPluginServiceConnector_nativeOnServiceConnectedCallback(
		JNIEnv *env, jobject thiz, jstring servicePackageName) {
	auto s = jstringToStdString(env, servicePackageName);
	aap::AAPJniFacade::getInstance()->handleServiceConnectedCallback(s);
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



aap::PluginListSnapshot cached_plugin_list{};


extern "C"
JNIEXPORT jlong JNICALL
Java_org_androidaudioplugin_hosting_NativePluginClient_newInstance(JNIEnv *env, jclass clazz,
																   jint serviceConnectionId) {
	auto connections = aap::AAPJniFacade::getInstance()->getPluginConnectionListFromJni(serviceConnectionId, true);
	assert(connections);
	if (cached_plugin_list.getNumPluginInformation() == 0)
		cached_plugin_list = aap::PluginListSnapshot::queryServices();
	return (jlong) (void*) new aap::PluginClient(connections, &cached_plugin_list);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativePluginClient_destroyInstance(JNIEnv *env, jclass clazz,
																	   jlong native) {
	auto client = (aap::PluginClient*) native;
	delete client;
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
    instance->prepare(frameCount);
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
																	   jint frameCount,
																	   jlong timeout_in_nanoseconds) {
    auto client = (aap::PluginClient*) (void*) nativeClient;
    auto instance = client->getInstanceById(instanceId);
	instance->process(frameCount, timeout_in_nanoseconds);
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
	return (jint) instance->getStandardExtensions().getMidiMappingPolicy();
}

// GUI extension

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_createGui(JNIEnv *env,
																	   jclass clazz,
																	   jlong nativeClient,
																	   jint instanceId) {
	auto client = (aap::PluginClient *) (void *) nativeClient;
	auto instance = client->getInstanceById(instanceId);
	// AudioPluginView is not provided from host side.
	return instance->getStandardExtensions().createGui(instance->getPluginInformation()->getPluginID(), instanceId, nullptr);
}

extern "C"
JNIEXPORT int32_t JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_showGui(JNIEnv *env,
																	   jclass clazz,
																	   jlong nativeClient,
																	   jint instanceId,
																	   jint guiInstanceId) {
	auto client = (aap::PluginClient *) (void *) nativeClient;
	auto instance = client->getInstanceById(instanceId);
	return instance->getStandardExtensions().showGui(guiInstanceId);
}

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_hideGui(JNIEnv *env,
																	   jclass clazz,
																	   jlong nativeClient,
																	   jint instanceId,
																	   jint guiInstanceId) {
	auto client = (aap::PluginClient *) (void *) nativeClient;
	auto instance = client->getInstanceById(instanceId);
	return instance->getStandardExtensions().hideGui(guiInstanceId);
}

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_resizeGui(JNIEnv *env,
																		  jclass clazz,
																		  jlong nativeClient,
																		  jint instanceId,
																		  jint guiInstanceId,
																		  jint width,
																		  jint height) {
	auto client = (aap::PluginClient *) (void *) nativeClient;
	auto instance = client->getInstanceById(instanceId);
	return instance->getStandardExtensions().resizeGui(guiInstanceId, width, height);
}

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_destroyGui(JNIEnv *env,
																	   jclass clazz,
																	   jlong nativeClient,
																	   jint instanceId,
																	   jint guiInstanceId) {
	auto client = (aap::PluginClient *) (void *) nativeClient;
	auto instance = client->getInstanceById(instanceId);
	return instance->getStandardExtensions().destroyGui(guiInstanceId);
}


extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_addEventUmpInput(JNIEnv *env,
																			jclass clazz,
																			jlong nativeClient,
																			jint instanceId,
																			jobject data,
																			jint length) {
	auto client = (aap::PluginClient *) (void *) nativeClient;
	auto instance = client->getInstanceById(instanceId);
	auto buf = env->GetDirectBufferAddress(data);
    instance->addEventUmpInput(buf, length);
}

// plugin instance (dynamic) information retrieval

jint implPluginHostGetParameterCount(jlong nativeHost,
                                     jint instanceId) {
    auto host = (aap::PluginHost *) (void *) nativeHost;
    auto instance = host->getInstanceById(instanceId);
    return instance->getNumParameters();
}

jint implPluginHostGetPortCount(jlong nativeHost,
                                jint instanceId) {
    auto host = (aap::PluginHost*) (void*) nativeHost;
    auto instance = host->getInstanceById(instanceId);
    return instance->getNumPorts();
}

const std::string& implPluginHostGetPluginId(jlong nativeHost,
                                      jint instanceId) {
    auto host = (aap::PluginHost*) (void*) nativeHost;
    auto instance = host->getInstanceById(instanceId);
    return instance->getPluginInformation()->getPluginID();
}

extern "C"
JNIEXPORT jlong JNICALL
Java_org_androidaudioplugin_AudioPluginServiceHelper_getServiceInstance(JNIEnv* env,
																		  jclass,
																		  jstring pluginId) {
	std::string pluginIdString = jstringToStdString(env, pluginId);
	auto svc = aap::PluginServiceList::getInstance()->findBoundServiceInProcess(pluginIdString.c_str());
	return (jlong) (void*) svc;
}

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_NativeLocalPluginInstance_getParameterCount(JNIEnv*,
                                                                        jclass ,
                                                                        jlong nativeService,
                                                                        jint instanceId) {
    return implPluginHostGetParameterCount(nativeService, instanceId);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_org_androidaudioplugin_NativeLocalPluginInstance_getParameter(JNIEnv *, jclass,
                                                                            jlong nativeClient,
                                                                            jint instanceId,
                                                                            jint index) {
	return aap::AAPJniFacade::getInstance()->getPluginInstanceParameter(nativeClient, instanceId, index);
}

extern "C"
JNIEXPORT jstring JNICALL
Java_org_androidaudioplugin_NativeLocalPluginInstance_getPluginId(JNIEnv* env,
                                                                   jclass ,
                                                                   jlong nativeService,
                                                                   jint instanceId) {
    return env->NewStringUTF(implPluginHostGetPluginId(nativeService, instanceId).c_str());
}

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_NativeLocalPluginInstance_getPortCount(JNIEnv*,
                                                                        jclass ,
                                                                        jlong nativeService,
                                                                        jint instanceId) {
    return implPluginHostGetPortCount(nativeService, instanceId);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_org_androidaudioplugin_NativeLocalPluginInstance_getPort(JNIEnv *env,
                                                              jclass ,
                                                              jlong nativeService,
                                                              jint instanceId,
                                                              jint index) {
	return aap::AAPJniFacade::getInstance()->getPluginInstancePort(nativeService, instanceId, index);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_NativeLocalPluginInstance_addEventUmpInput(JNIEnv *env, jclass clazz,
                                                                       jlong nativeService,
                                                                       jint instanceId,
                                                                       jobject data, jint size) {
    auto host = (aap::PluginService *) (void *) nativeService;
    auto instance = host->getInstanceById(instanceId);
    instance->addEventUmpInput(env->GetDirectBufferAddress(data), size);
}

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getParameterCount(JNIEnv *,
																			jclass clazz,
																			jlong nativeClient,
																			jint instanceId) {
    return implPluginHostGetParameterCount(nativeClient, instanceId);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getParameter(JNIEnv *, jclass ,
																	   jlong nativeClient,
																	   jint instanceId,
																	   jint index) {
	return aap::AAPJniFacade::getInstance()->getPluginInstanceParameter(nativeClient, instanceId, index);
}

extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getPortCount(JNIEnv *,
                                                                            jclass ,
                                                                            jlong nativeClient,
                                                                            jint instanceId) {
    return implPluginHostGetPortCount(nativeClient, instanceId);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_getPort(JNIEnv *env, jclass ,
																	   jlong nativeClient,
																	   jint instanceId,
																	   jint index) {
	return aap::AAPJniFacade::getInstance()->getPluginInstancePort(nativeClient, instanceId, index);
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
    auto b = instance->getAudioPluginBuffer();
	auto src = b->get_buffer(*b, portIndex);
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
    auto b = instance->getAudioPluginBuffer();
	auto dst = b->get_buffer(*b, portIndex);
	memcpy(dst, src, size);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_NativeRemotePluginInstance_00024Companion_sendExtensionRequest(
        JNIEnv *env, jobject thiz, jlong nativeClient, jint instanceId, jstring uri,
        jint opcode, jobject buffer, jint offset, jint length) {
    auto client = (aap::PluginClient *) (void *) nativeClient;
    auto instance = (aap::RemotePluginInstance*) client->getInstanceById(instanceId);
    jboolean isCopy{false};
    auto uriChars = env->GetStringUTFChars(uri, &isCopy);
    auto src = (uint8_t*) env->GetDirectBufferAddress(buffer);
    instance->sendPluginAAPXSRequest(0, uriChars, opcode, src + offset, length,
                                     instance->aapxsRequestIdSerial());
    if (isCopy)
        env->ReleaseStringChars(uri, (const jchar*) uriChars);
}


extern "C"
JNIEXPORT jint JNICALL
Java_org_androidaudioplugin_NativeLocalPluginInstance_getPresetCount(JNIEnv*,
                                                                     jclass ,
                                                                     jlong nativeService,
                                                                     jint instanceId) {
    auto service = (aap::PluginService *) (void *) nativeService;
    auto instance = service->getInstanceById(instanceId);
    return (jint) instance->getStandardExtensions().getPresetCount();
}

extern "C"
JNIEXPORT jstring JNICALL
Java_org_androidaudioplugin_NativeLocalPluginInstance_getPresetName(JNIEnv* env,
                                                                     jclass ,
                                                                     jlong nativeService,
                                                                     jint instanceId,
                                                                     jint index) {
    auto service = (aap::PluginService *) (void *) nativeService;
    auto instance = service->getInstanceById(instanceId);
    std::string name = instance->getStandardExtensions().getPresetName(index);
    return env->NewStringUTF(name.c_str());
}

extern "C"
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_NativeLocalPluginInstance_setPresetIndex(JNIEnv *env,
                                                                    jclass clazz,
                                                                    jlong nativeService,
                                                                    jint instanceId, jint index) {
    auto service = (aap::PluginService *) (void *) nativeService;
    auto instance = service->getInstanceById(instanceId);
    instance->getStandardExtensions().setCurrentPresetIndex(index);
}
