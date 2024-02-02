#include "aap/core/android/android-application-context.h"
#include "../core/AAPJniFacade.h"
#include "ALooperMessage.h"
#include "aap/core/host/audio-plugin-host.h"

namespace aap {
    const char *java_plugin_information_class_name = "org/androidaudioplugin/PluginInformation",
            *java_extension_information_class_name = "org/androidaudioplugin/ExtensionInformation",
            *java_parameter_information_class_name = "org/androidaudioplugin/ParameterInformation",
            *java_enumeration_information_class_name = "org/androidaudioplugin/ParameterInformation$EnumerationInformation",
            *java_port_information_class_name = "org/androidaudioplugin/PortInformation",
            *java_audio_plugin_service_class_name = "org/androidaudioplugin/AudioPluginServiceHelper",
            *java_native_remote_plugin_instance_class_name = "org/androidaudioplugin/hosting/NativeRemotePluginInstance";

    static jmethodID
            j_method_is_out_process,
            j_method_get_package_name,
            j_method_get_local_name,
            j_method_get_name,
            j_method_get_developer,
            j_method_get_version,
            j_method_get_plugin_id,
            j_method_get_shared_library_filename,
            j_method_get_library_entrypoint,
            j_method_get_category,
            j_method_get_ui_view_factory,
            j_method_get_ui_activity,
            j_method_get_ui_web,
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
            j_method_parameter_add_enum,
            j_method_parameter_get_enumeration_count,
            j_method_parameter_get_enumeration,
            j_method_enumeration_get_value,
            j_method_enumeration_get_name,
            j_method_port_ctor,
            j_method_port_get_index,
            j_method_port_get_name,
            j_method_port_get_direction,
            j_method_port_get_content,
            j_method_port_get_minimum_size_in_bytes,
            j_method_remote_plugin_instance_create_from_native;

    void AAPJniFacade::initializeJNIMetadata() {
        JNIEnv *env;
        auto vm = aap::get_android_jvm();
        if (vm == nullptr) {
            AAP_ASSERT_FALSE;
            return;
        }
        vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
        if (env == nullptr) {
            AAP_ASSERT_FALSE;
            return;
        }

        jclass java_plugin_information_class = env->FindClass(java_plugin_information_class_name),
                java_extension_information_class = env->FindClass(
                    java_extension_information_class_name),
                java_parameter_information_class = env->FindClass(
                    java_parameter_information_class_name),
                java_enumeration_information_class = env->FindClass(
                    java_enumeration_information_class_name),
                java_port_information_class = env->FindClass(java_port_information_class_name),
                java_native_remote_plugin_instance_class = env->FindClass(
                    java_native_remote_plugin_instance_class_name);

        j_method_is_out_process = env->GetMethodID(java_plugin_information_class,
                                                   "isOutProcess", "()Z");
        j_method_get_package_name = env->GetMethodID(java_plugin_information_class,
                                                     "getPackageName",
                                                     "()Ljava/lang/String;");
        j_method_get_local_name = env->GetMethodID(java_plugin_information_class, "getLocalName",
                                                   "()Ljava/lang/String;");
        j_method_get_name = env->GetMethodID(java_plugin_information_class, "getDisplayName",
                                             "()Ljava/lang/String;");
        j_method_get_developer = env->GetMethodID(java_plugin_information_class,
                                                  "getDeveloper", "()Ljava/lang/String;");
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
        j_method_get_ui_view_factory = env->GetMethodID(java_plugin_information_class,
                                                        "getUiViewFactory",
                                                        "()Ljava/lang/String;");
        j_method_get_ui_activity = env->GetMethodID(java_plugin_information_class,
                                                    "getUiActivity",
                                                    "()Ljava/lang/String;");
        j_method_get_ui_web = env->GetMethodID(java_plugin_information_class,
                                               "getUiWeb",
                                               "()Ljava/lang/String;");
        j_method_get_extension_count = env->GetMethodID(java_plugin_information_class,
                                                        "getExtensionCount", "()I");
        j_method_get_extension = env->GetMethodID(java_plugin_information_class, "getExtension",
                                                  "(I)Lorg/androidaudioplugin/ExtensionInformation;");

        j_method_extension_get_required = env->GetMethodID(java_extension_information_class,
                                                           "getRequired",
                                                           "()Z");
        j_method_extension_get_uri = env->GetMethodID(java_extension_information_class, "getUri",
                                                      "()Ljava/lang/String;");

        j_method_get_declared_parameter_count = env->GetMethodID(java_plugin_information_class,
                                                                 "getDeclaredParameterCount",
                                                                 "()I");
        j_method_get_declared_parameter = env->GetMethodID(java_plugin_information_class,
                                                           "getDeclaredParameter",
                                                           "(I)Lorg/androidaudioplugin/ParameterInformation;");
        j_method_get_declared_port_count = env->GetMethodID(java_plugin_information_class,
                                                            "getDeclaredPortCount", "()I");
        j_method_get_declared_port = env->GetMethodID(java_plugin_information_class,
                                                      "getDeclaredPort",
                                                      "(I)Lorg/androidaudioplugin/PortInformation;");

        j_method_parameter_ctor = env->GetMethodID(java_parameter_information_class, "<init>",
                                                   "(ILjava/lang/String;DDD)V");
        j_method_parameter_get_id = env->GetMethodID(java_parameter_information_class, "getId",
                                                     "()I");
        j_method_parameter_get_name = env->GetMethodID(java_parameter_information_class, "getName",
                                                       "()Ljava/lang/String;");
        j_method_parameter_get_default_value = env->GetMethodID(java_parameter_information_class,
                                                                "getDefaultValue",
                                                                "()D");
        j_method_parameter_get_minimum_value = env->GetMethodID(java_parameter_information_class,
                                                                "getMinimumValue",
                                                                "()D");
        j_method_parameter_get_maximum_value = env->GetMethodID(java_parameter_information_class,
                                                                "getMaximumValue",
                                                                "()D");
        j_method_parameter_add_enum = env->GetMethodID(java_parameter_information_class,
                                                       "addEnum",
                                                       "(IDLjava/lang/String;)V");
        j_method_parameter_get_enumeration_count = env->GetMethodID(java_parameter_information_class, "getEnumCount", "()I");
        j_method_parameter_get_enumeration = env->GetMethodID(java_parameter_information_class, "getEnum",
                                                              "(I)Lorg/androidaudioplugin/ParameterInformation$EnumerationInformation;");
        j_method_enumeration_get_value = env->GetMethodID(java_enumeration_information_class, "getValue", "()D");

        j_method_enumeration_get_name = env->GetMethodID(java_enumeration_information_class, "getName", "()Ljava/lang/String;");

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
        j_method_port_get_minimum_size_in_bytes = env->GetMethodID(java_port_information_class,
                                                                   "getMinimumSizeInBytes",
                                                                   "()I");
        j_method_remote_plugin_instance_create_from_native = env->GetStaticMethodID(java_native_remote_plugin_instance_class,
                                                                                    "fromNative",
                                                                                    "(IJ)Lorg/androidaudioplugin/hosting/NativeRemotePluginInstance;");
    }

// FIXME: at some stage we should reorganize this JNI part so that "audio-plugin-host" part and
//  AAPXS part can live apart. Probably we use some decent JNI helper library
//  (libnativehelper, fbjni, JMI, or whatever).

    template <typename T>
    T usingJNIEnv(std::function<T(JNIEnv*)> func) {
        JNIEnv *env;
        JavaVM* vm = aap::get_android_jvm();
        if (vm == nullptr) {
            AAP_ASSERT_FALSE;
            return (T) 0;
        }
        auto envState = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (envState == JNI_EDETACHED)
            vm->AttachCurrentThread(&env, nullptr);
        if (env == nullptr) {
            AAP_ASSERT_FALSE;
            return (T) 0;
        }

        T ret = func(env);

        if (envState == JNI_EDETACHED)
            vm->DetachCurrentThread();
        return ret;
    }


//----------------------------------------------

    std::unique_ptr<AAPJniFacade> jni_facade_instance{nullptr};

    AAPJniFacade *AAPJniFacade::getInstance() {
        if (!jni_facade_instance)
            jni_facade_instance = std::make_unique<AAPJniFacade>();
        return jni_facade_instance.get();
    }

    const char *keepPointer(std::vector<const char *> freeList, const char *ptr) {
        freeList.emplace_back(ptr);
        return ptr;
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

    aap::PluginInformation *
    AAPJniFacade::pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation) {
        std::vector<const char *> freeList{};
        auto aapPI = new aap::PluginInformation(
                env->CallBooleanMethod(pluginInformation, j_method_is_out_process),
                keepPointer(freeList,
                            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                                 j_method_get_package_name))),
                keepPointer(freeList,
                            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                                 j_method_get_local_name))),
                keepPointer(freeList,
                            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                                 j_method_get_name))),
                keepPointer(freeList,
                            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                                 j_method_get_developer))),
                keepPointer(freeList,
                            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                                 j_method_get_version))),
                keepPointer(freeList,
                            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                                 j_method_get_plugin_id))),
                keepPointer(freeList,
                            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                                 j_method_get_shared_library_filename))),
                keepPointer(freeList,
                            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                                 j_method_get_library_entrypoint))),
                "", // metadataFullPath, no use on Android
                keepPointer(freeList,
                            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                                 j_method_get_category))),
                keepPointer(freeList,
                            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                                 j_method_get_ui_view_factory))),
                keepPointer(freeList,
                            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                                 j_method_get_ui_activity))),
                keepPointer(freeList,
                            strdup_fromJava(env, (jstring) env->CallObjectMethod(pluginInformation,
                                                                                 j_method_get_ui_web)))
        );
        for (auto p: freeList)
            free((void *) p);

        int nExtensions = env->CallIntMethod(pluginInformation, j_method_get_extension_count);
        for (int i = 0; i < nExtensions; i++) {
            jobject port = env->CallObjectMethod(pluginInformation, j_method_get_extension, i);
            auto required = (bool) env->CallBooleanMethod(port, j_method_extension_get_required);
            auto name = strdup_fromJava(env,
                                        (jstring) env->CallObjectMethod(port,
                                                                        j_method_extension_get_uri));
            aapPI->addExtension(aap::PluginExtensionInformation{required, name});
            free((void *) name);
        }

        int nParameters = env->CallIntMethod(pluginInformation,
                                             j_method_get_declared_parameter_count);
        for (int i = 0; i < nParameters; i++) {
            jobject para = env->CallObjectMethod(pluginInformation, j_method_get_declared_parameter,
                                                 i);
            auto id = (uint32_t) env->CallIntMethod(para, j_method_parameter_get_id);
            auto name = strdup_fromJava(env, (jstring) env->CallObjectMethod(para,
                                                                             j_method_parameter_get_name));
            auto def = env->CallDoubleMethod(para, j_method_parameter_get_default_value);
            auto min = env->CallDoubleMethod(para, j_method_parameter_get_minimum_value);
            auto max = env->CallDoubleMethod(para, j_method_parameter_get_maximum_value);

            auto nativePara = new aap::ParameterInformation(id, name, min, max, def);

            int32_t nEnums = env->CallIntMethod(para, j_method_parameter_get_enumeration_count);
            for (int e = 0; e < nEnums; e++) {
                auto eObj = env->CallObjectMethod(para, j_method_parameter_get_enumeration, e);
                auto eValue = env->CallDoubleMethod(eObj, j_method_enumeration_get_value);
                auto eName = strdup_fromJava(env, (jstring) env->CallObjectMethod(eObj,
                                                                                 j_method_enumeration_get_name));
                ParameterInformation::Enumeration nativeEnum{e, eValue, eName};
                nativePara->addEnumeration(nativeEnum);
            }

            aapPI->addDeclaredParameter(nativePara);
            free((void *) name);
        }

        int nPorts = env->CallIntMethod(pluginInformation, j_method_get_declared_port_count);
        for (int i = 0; i < nPorts; i++) {
            jobject port = env->CallObjectMethod(pluginInformation, j_method_get_declared_port, i);
            auto index = (uint32_t) env->CallIntMethod(port, j_method_port_get_index);
            auto name = strdup_fromJava(env, (jstring) env->CallObjectMethod(port,
                                                                             j_method_port_get_name));
            auto content = (aap_content_type) (int) env->CallIntMethod(port,
                                                                       j_method_port_get_content);
            auto direction = (aap_port_direction) (int) env->CallIntMethod(port,
                                                                           j_method_port_get_direction);
            auto nativePort = new aap::PortInformation(index, name, content, direction);
            auto minSize = env->CallIntMethod(port, j_method_port_get_minimum_size_in_bytes);
            if (minSize != 0)
                nativePort->setPropertyValueString(AAP_PORT_MINIMUM_SIZE, std::to_string(minSize));
            aapPI->addDeclaredPort(nativePort);
            free((void *) name);
        }

        return aapPI;
    }


// --------------------------------------------------

    jobjectArray AAPJniFacade::queryInstalledPluginsJNI() {
        return usingJNIEnv<jobjectArray>([](JNIEnv *env) {
            jclass java_audio_plugin_host_helper_class = env->FindClass(
                    "org/androidaudioplugin/hosting/AudioPluginHostHelper");
            if (!java_audio_plugin_host_helper_class)
                AAP_ASSERT_FALSE; // ... and leave WTF JNI causes.
            jmethodID j_method_query_audio_plugins = env->GetStaticMethodID(
                    java_audio_plugin_host_helper_class, "queryAudioPlugins",
                    "(Landroid/content/Context;)[Lorg/androidaudioplugin/PluginInformation;");
            if (!j_method_query_audio_plugins)
                AAP_ASSERT_FALSE; // ... and leave WTF JNI causes.
            return (jobjectArray) env->CallStaticObjectMethod(java_audio_plugin_host_helper_class,
                                                              j_method_query_audio_plugins,
                                                              aap::get_android_application_context());
        });
    }

// --------------------------------------------------

    int32_t AAPJniFacade::getMidiSettingsFromLocalConfig(std::string pluginId) {
        return usingJNIEnv<int32_t>([pluginId](JNIEnv *env) {
            auto java_audio_plugin_midi_settings_class = AAPJniFacade::getInstance()->getAudioPluginMidiSettingsClass();
            if (!java_audio_plugin_midi_settings_class)
                AAP_ASSERT_FALSE; // ... and leave WTF JNI causes.
            jmethodID j_method_get_midi_settings_from_shared_preference = env->GetStaticMethodID(
                    java_audio_plugin_midi_settings_class, "getMidiSettingsFromSharedPreference",
                    "(Landroid/content/Context;Ljava/lang/String;)I");
            if (!j_method_get_midi_settings_from_shared_preference)
                AAP_ASSERT_FALSE; // ... and leave WTF JNI causes.
            auto context = aap::get_android_application_context();
            auto pluginIdJString = env->NewStringUTF(pluginId.c_str());
            return env->CallStaticIntMethod(java_audio_plugin_midi_settings_class,
                                            j_method_get_midi_settings_from_shared_preference,
                                            context, pluginIdJString);
        });
    }

    void AAPJniFacade::putMidiSettingsToSharedPreference(std::string pluginId, int32_t flags) {
        usingJNIEnv<int32_t>([this, pluginId, flags](JNIEnv *env) {
            auto java_audio_plugin_midi_settings_class = getAudioPluginMidiSettingsClass();
            if (!java_audio_plugin_midi_settings_class)
                AAP_ASSERT_FALSE; // ... and leave WTF JNI causes.
            jmethodID j_method_put_midi_settings_to_shared_preference = env->GetStaticMethodID(
                    java_audio_plugin_midi_settings_class, "putMidiSettingsToSharedPreference",
                    "(Landroid/content/Context;Ljava/lang/String;I)V");
            if (!j_method_put_midi_settings_to_shared_preference)
                AAP_ASSERT_FALSE; // ... and leave WTF JNI causes.
            auto context = aap::get_android_application_context();
            auto pluginIdJString = env->NewStringUTF(pluginId.c_str());
            env->CallStaticVoidMethod(java_audio_plugin_midi_settings_class,
                                      j_method_put_midi_settings_to_shared_preference, context,
                                      pluginIdJString, flags);
            return 0;
        });
    }

// --------------------------------------------------

    template<typename T> T usingContext(std::function<T(JNIEnv* env, jclass cls, jobject context)> func) {
        return usingJNIEnv<T>([&](JNIEnv* env) {
            auto context = aap::get_android_application_context();
            auto cls = env->FindClass(java_audio_plugin_service_class_name);
            if (!cls)
                AAP_ASSERT_FALSE; // ... and leave WTF JNI causes.
            return func(env, cls, context);
        });
    }

    int32_t gui_instance_id_serial;
    int32_t getGuiInstanceSerial() { return gui_instance_id_serial++; }

    void* AAPJniFacade::getRemoteWebView(PluginClient* client, RemotePluginInstance* instance) {
        return usingJNIEnv<jobject>([client,instance](JNIEnv* env) {
            auto clzInstance = env->FindClass(java_native_remote_plugin_instance_class_name);
            auto jniNativePeer = env->CallStaticObjectMethod(clzInstance, j_method_remote_plugin_instance_create_from_native, instance->getInstanceId(), (jlong) client);
            auto clzHelper = env->FindClass("org/androidaudioplugin/ui/web/WebUIHostHelper");
            if (env->ExceptionOccurred()) {
                env->ExceptionDescribe();
                return (jobject) nullptr;
            }
            auto getWebView = env->GetStaticMethodID(clzHelper, "getWebView", "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Lorg/androidaudioplugin/hosting/NativeRemotePluginInstance;Z)Landroid/webkit/WebView;");
            if (env->ExceptionOccurred()) {
                env->ExceptionDescribe();
                return (jobject) nullptr;
            }
            auto jPluginId = env->NewStringUTF(instance->getPluginInformation()->getPluginID().c_str());
            auto jPackageName = env->NewStringUTF(instance->getPluginInformation()->getPluginPackageName().c_str());
            return env->CallStaticObjectMethod(clzHelper, getWebView, aap::get_android_application_context(), jPluginId, jPackageName, jniNativePeer, false);
        });
    }

    void* AAPJniFacade::createSurfaceControl() {
        return usingJNIEnv<jobject>([](JNIEnv* env) {
            auto clzHelper = env->FindClass("org/androidaudioplugin/hosting/AudioPluginHostHelper");
            if (env->ExceptionOccurred()) {
                env->ExceptionDescribe();
                return (jobject) nullptr;
            }
            auto createSurfaceControl = env->GetStaticMethodID(clzHelper, "createSurfaceControl", "(Landroid/content/Context;)Lorg/androidaudioplugin/hosting/AudioPluginSurfaceControlClient;");
            if (env->ExceptionOccurred()) {
                env->ExceptionDescribe();
                return (jobject) nullptr;
            }
            auto controlHost = env->CallStaticObjectMethod(clzHelper, createSurfaceControl, aap::get_android_application_context());
            return env->NewGlobalRef(controlHost);
        });
    }

    void AAPJniFacade::disposeSurfaceControl(void* handle) {
    }

    void AAPJniFacade::showSurfaceControlView(void* handle) {
    }

    void AAPJniFacade::hideSurfaceControlView(void* handle) {
    }


    void* AAPJniFacade::getRemoteNativeView(PluginClient* client, RemotePluginInstance* instance) {

        return usingJNIEnv<jobject>([instance](JNIEnv* env) {
            auto clzControl = env->FindClass("org/androidaudioplugin/hosting/AudioPluginSurfaceControlClient");
            if (env->ExceptionOccurred()) {
                env->ExceptionDescribe();
                return (jobject) nullptr;
            }
            auto getSurfaceView = env->GetMethodID(clzControl, "getSurfaceView", "()Landroid/view/View;");
            if (env->ExceptionOccurred()) {
                env->ExceptionDescribe();
                return (jobject) nullptr;
            }
            auto uiController = instance->getNativeUIController();
            if (!uiController) {
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPJniFacade", "createSurfaceControl() was not invoked yet.");
                return (jobject) nullptr;
            }
            auto controlClient = (jobject) uiController->getHandle();
            if (!controlClient) {
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPJniFacade", "Native UI controller does not exist. Maybe the UI factory is not configured properly?");
                return (jobject) nullptr;
            }
            return env->CallObjectMethod((jobject) controlClient, getSurfaceView);
        });
    }

    void AAPJniFacade::connectRemoteNativeView(PluginClient* client, RemotePluginInstance* instance, int32_t width, int32_t height) {

        usingJNIEnv<int32_t>([instance, width, height](JNIEnv* env) {
            auto clzControl = env->FindClass("org/androidaudioplugin/hosting/AudioPluginSurfaceControlClient");
            if (env->ExceptionOccurred()) {
                env->ExceptionDescribe();
                return 0;
            }
            auto connectUIAsync = env->GetMethodID(clzControl, "connectUIAsync", "(Ljava/lang/String;Ljava/lang/String;III)V");
            if (env->ExceptionOccurred()) {
                env->ExceptionDescribe();
                return 0;
            }
            auto uiController = instance->getNativeUIController();
            if (!uiController) {
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPJniFacade", "createSurfaceControl() was not invoked yet.");
                return 0;
            }
            auto controlClient = (jobject) uiController->getHandle();
            if (!controlClient) {
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPJniFacade", "Native UI controller does not exist. Maybe the UI factory is not configured properly?");
                return 0;
            }
            auto jPluginId = env->NewStringUTF(instance->getPluginInformation()->getPluginID().c_str());
            auto jPackageName = env->NewStringUTF(instance->getPluginInformation()->getPluginPackageName().c_str());
            env->CallVoidMethod((jobject) controlClient, connectUIAsync, jPackageName, jPluginId, instance->getInstanceId(), width, height);
            return 0;
        });
    }

// --------------------------------------------------

#define CLEAR_JNI_ERROR(env) { if (env->ExceptionOccurred()) { env->ExceptionDescribe(); env->ExceptionClear(); } }

// It is required where the current Dalvik thread cannot resolve classes from current Context (app)...
// JNI_OnLoad() at AudioPluginService does not seem to work enough to resolve classes in the apk, unlike
// explained at https://developer.android.com/training/articles/perf-jni#faq:-why-didnt-findclass-find-my-class
    jclass AAPJniFacade::getAudioPluginMidiSettingsClass() {
        JNIEnv *env;
        aap::get_android_jvm()->GetEnv((void **) &env, JNI_VERSION_1_6);

        auto context = aap::get_android_application_context();
        auto contextClass = env->FindClass("android/content/Context");
        auto getClassLoader = env->GetMethodID(contextClass, "getClassLoader",
                                               "()Ljava/lang/ClassLoader;");
        auto contextClassLoader = env->CallObjectMethod(context, getClassLoader);

        auto classLoaderClass = env->FindClass("java/lang/ClassLoader");
        CLEAR_JNI_ERROR(env)
        if (!classLoaderClass)
            AAP_ASSERT_FALSE; // ... and leave WTF JNI causes.
        auto findClassMethod = env->GetMethodID(classLoaderClass, "findClass",
                                                "(Ljava/lang/String;)Ljava/lang/Class;");
        CLEAR_JNI_ERROR(env)
        if (!findClassMethod)
            AAP_ASSERT_FALSE; // ... and leave WTF JNI causes.
        auto settingsClassObj = env->CallObjectMethod(contextClassLoader, findClassMethod,
                                                      env->NewStringUTF(
                                                              "org/androidaudioplugin/hosting/AudioPluginMidiSettings"));
        CLEAR_JNI_ERROR(env)
        if (!settingsClassObj)
            AAP_ASSERT_FALSE; // ... and leave WTF JNI causes.
        return (jclass) settingsClassObj;
    }

// --------------------------------------------------

    jobject audio_plugin_service_connector{nullptr};
    std::map<std::string, std::function<void(std::string &)> > inProgressCallbacks{};

    void AAPJniFacade::ensureServiceConnectedFromJni(jint connectorInstanceId,
                                                     std::string servicePackageName,
                                                     std::function<void(std::string &)> callback) {
        inProgressCallbacks[servicePackageName] = callback;

        usingJNIEnv<void *>([&](JNIEnv *env) {

            if (audio_plugin_service_connector == nullptr) {
                jclass connector_class = env->FindClass(
                        "org/androidaudioplugin/hosting/AudioPluginServiceConnector");
                jmethodID constructor = env->GetMethodID(connector_class, "<init>",
                                                         "(Landroid/content/Context;)V");
                audio_plugin_service_connector = env->NewGlobalRef(
                        env->NewObject(connector_class, constructor,
                                       aap::get_android_application_context()));
            }

            jclass java_audio_plugin_host_helper_class = env->FindClass(
                    "org/androidaudioplugin/hosting/AudioPluginHostHelper");
            if (!java_audio_plugin_host_helper_class)
                AAP_ASSERT_FALSE; // ... and leave WTF JNI causes.
            jmethodID j_method_ensure_instance_created = env->GetStaticMethodID(
                    java_audio_plugin_host_helper_class, "ensureBinderConnected",
                    "(Ljava/lang/String;Lorg/androidaudioplugin/hosting/AudioPluginServiceConnector;)V");
            if (!j_method_ensure_instance_created)
                AAP_ASSERT_FALSE; // ... and leave WTF JNI causes.

            env->CallStaticVoidMethod(java_audio_plugin_host_helper_class,
                                      j_method_ensure_instance_created,
                                      env->NewStringUTF(servicePackageName.c_str()),
                                      audio_plugin_service_connector);
            return nullptr;
        });
    }

    void AAPJniFacade::handleServiceConnectedCallback(std::string servicePackageName) {
        auto entry = inProgressCallbacks.find(servicePackageName);
        if (entry != inProgressCallbacks.end()) {
            // FIXME: what kind of error propagation could be achieved here?
            std::string empty{};
            entry->second(empty);
        }
    }

    // JNI helper (that they are not really call into Java but moved here.

    jobject AAPJniFacade::getPluginInstanceParameter(jlong nativeHost, jint instanceId,jint index) {
        return usingJNIEnv<jobject>([&](JNIEnv* env) {
            auto host = (aap::PluginHost *) (void *) nativeHost;
            auto instance = host->getInstanceById(instanceId);
            auto para = instance->getParameter(index);
            auto klass = env->FindClass(java_parameter_information_class_name);
            if (!klass)
                AAP_ASSERT_FALSE; // ... and leave WTF JNI causes.
            auto ret = env->NewObject(klass, j_method_parameter_ctor, (jint) para->getId(),
                                  env->NewStringUTF(para->getName()), para->getMinimumValue(),
                                  para->getMaximumValue(), para->getDefaultValue());
            for (int32_t i = 0, n = para->getEnumCount(); i < n; i++) {
                auto en = para->getEnumeration(i);
                env->CallVoidMethod(ret, j_method_parameter_add_enum,
                                    en.getIndex(), en.getValue(), env->NewStringUTF(en.getName().c_str()));
            }
            return ret;
        });
    }

    jobject AAPJniFacade::getPluginInstancePort(jlong nativeHost, jint instanceId, jint index) {
        return usingJNIEnv<jobject>([&](JNIEnv* env) {
            auto host = (aap::PluginHost *) (void *) nativeHost;
            auto instance = host->getInstanceById(instanceId);
            auto port = instance->getPort(index);
            auto klass = env->FindClass(java_port_information_class_name);
            if (!klass)
                AAP_ASSERT_FALSE; // ... and leave WTF JNI causes.
            return env->NewObject(klass, j_method_port_ctor, (jint) port->getIndex(),
                                  env->NewStringUTF(port->getName()),
                                  (jint) port->getPortDirection(),
                                  (jint) port->getContentType());
        });
    }

// --------------------------------------------------

    std::map<jint, aap::PluginClientConnectionList*> client_connection_list_per_scope{};

    int32_t AAPJniFacade::getConnectorInstanceId(aap::PluginClientConnectionList* connections) {
        for (auto entry : client_connection_list_per_scope)
            if (entry.second == connections)
                return entry.first;
        return -1;
    }

    aap::PluginClientConnectionList* AAPJniFacade::getPluginConnectionListFromJni(jint connectorInstanceId, bool createIfNotExist) {
        if (client_connection_list_per_scope.find(connectorInstanceId) != client_connection_list_per_scope.end())
            return client_connection_list_per_scope[connectorInstanceId];
        if (!createIfNotExist)
            return nullptr;
        auto ret = new aap::PluginClientConnectionList();
        client_connection_list_per_scope[connectorInstanceId] = ret;
        return ret;
    }

    void AAPJniFacade::addScopedClientConnection(int32_t connectorInstanceId, std::string packageName, std::string className, void* connectionData) {
        auto list = client_connection_list_per_scope[connectorInstanceId];
        if (list == nullptr) {
            client_connection_list_per_scope[connectorInstanceId] = new aap::PluginClientConnectionList();
            list = client_connection_list_per_scope[connectorInstanceId];
        }
        list->add(std::make_unique<aap::PluginClientConnection>(packageName, className, connectionData));
    }

    void AAPJniFacade::removeScopedClientConnection(int32_t connectorInstanceId, std::string packageName, std::string className) {
        auto list = client_connection_list_per_scope[connectorInstanceId];
        if (list != nullptr)
            list->remove(packageName.c_str(), className.c_str());
    }

    // extra: defined in audio-plugin-host-android.h for midi-device-service.
    // FIXME: maybe it should be exposed by some other means.
    aap::PluginClientConnectionList* getPluginConnectionListByConnectorInstanceId(int32_t connectorInstanceId, bool createIfNotExist) {
        return AAPJniFacade::getInstance()->getPluginConnectionListFromJni(connectorInstanceId, createIfNotExist);
    }

}