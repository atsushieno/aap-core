#include <cassert>
#include "aap/core/android/android-application-context.h"
#include "../core/AAPJniFacade.h"
#include "ALooperMessage.h"
#include "aap/core/host/audio-plugin-host.h"

namespace aap {
    const char *java_plugin_information_class_name = "org/androidaudioplugin/PluginInformation",
            *java_extension_information_class_name = "org/androidaudioplugin/ExtensionInformation",
            *java_parameter_information_class_name = "org/androidaudioplugin/ParameterInformation",
            *java_port_information_class_name = "org/androidaudioplugin/PortInformation",
            *java_audio_plugin_service_class_name = "org/androidaudioplugin/AudioPluginServiceHelper";

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
            j_method_port_ctor,
            j_method_port_get_index,
            j_method_port_get_name,
            j_method_port_get_direction,
            j_method_port_get_content,
            j_method_port_get_minimum_size_in_bytes,
            j_method_create_gui,
            j_method_show_gui,
            j_method_hide_gui,
            j_method_destroy_gui;

    void AAPJniFacade::initializeJNIMetadata() {
        JNIEnv *env;
        auto vm = aap::get_android_jvm();
        assert(vm);
        vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
        assert(env);

        jclass java_plugin_information_class = env->FindClass(java_plugin_information_class_name),
                java_extension_information_class = env->FindClass(
                java_extension_information_class_name),
                java_parameter_information_class = env->FindClass(
                java_parameter_information_class_name),
                java_port_information_class = env->FindClass(java_port_information_class_name),
                java_audio_plugin_service_helper_class = env->FindClass(
                java_audio_plugin_service_class_name);

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
        j_method_create_gui = env->GetStaticMethodID(java_audio_plugin_service_helper_class,
                                                     "createGui",
                                                     "(Landroid/content/Context;Ljava/lang/String;I)Lorg/androidaudioplugin/AudioPluginView;");
        j_method_show_gui = env->GetStaticMethodID(java_audio_plugin_service_helper_class,
                                                     "showGui",
                                                     "(Landroid/content/Context;Ljava/lang/String;ILorg/androidaudioplugin/AudioPluginView;)V");
        j_method_hide_gui = env->GetStaticMethodID(java_audio_plugin_service_helper_class,
                                                   "hideGui",
                                                   "(Landroid/content/Context;Ljava/lang/String;ILorg/androidaudioplugin/AudioPluginView;)V");
        j_method_destroy_gui = env->GetStaticMethodID(java_audio_plugin_service_helper_class,
                                                   "destroyGui",
                                                   "(Landroid/content/Context;Ljava/lang/String;ILorg/androidaudioplugin/AudioPluginView;)V");
    }

// FIXME: at some stage we should reorganize this JNI part so that "audio-plugin-host" part and
//  AAPXS part can live apart. Probably we use some decent JNI helper library
//  (libnativehelper, fbjni, JMI, or whatever).

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
            nativePort->setPropertyValueString(AAP_PORT_MINIMUM_SIZE, std::to_string(
                    env->CallIntMethod(port, j_method_port_get_minimum_size_in_bytes)));
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

    int32_t AAPJniFacade::getMidiSettingsFromLocalConfig(std::string pluginId) {
        return usingJNIEnv<int32_t>([pluginId](JNIEnv *env) {
            auto java_audio_plugin_midi_settings_class = AAPJniFacade::getInstance()->getAudioPluginMidiSettingsClass();
            assert(java_audio_plugin_midi_settings_class);
            jmethodID j_method_get_midi_settings_from_shared_preference = env->GetStaticMethodID(
                    java_audio_plugin_midi_settings_class, "getMidiSettingsFromSharedPreference",
                    "(Landroid/content/Context;Ljava/lang/String;)I");
            assert(j_method_get_midi_settings_from_shared_preference);
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
            assert(java_audio_plugin_midi_settings_class);
            jmethodID j_method_put_midi_settings_to_shared_preference = env->GetStaticMethodID(
                    java_audio_plugin_midi_settings_class, "putMidiSettingsToSharedPreference",
                    "(Landroid/content/Context;Ljava/lang/String;I)V");
            assert(j_method_put_midi_settings_to_shared_preference);
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
            assert(cls);
            return func(env, cls, context);
        });
    }

    int32_t gui_instance_id_serial;
    int32_t getGuiInstanceSerial() { return gui_instance_id_serial++; }

    class CreateGuiMessage : public ALooperMessage {
        GuiInstance* gui_instance;
        std::function<void()> callback;

    protected:
        void handleMessage() {
            usingContext<int>([&](JNIEnv* env, jclass cls, jobject context) {
                auto pluginIdJString = env->NewStringUTF(gui_instance->pluginId.c_str());
                auto ret = env->CallStaticObjectMethod(cls, j_method_create_gui, context, pluginIdJString,
                                                       gui_instance->instanceId);
                auto ex = env->ExceptionOccurred();
                if (ex) {
                    env->ExceptionDescribe();
                    gui_instance->lastError = "AudioPluginServiceHelper.createGui() failed. Check the Android log for details.";
                    aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPJniFacade", gui_instance->lastError.c_str());
                    env->ExceptionClear();
                }
                gui_instance->view = env->NewGlobalRef(ret);
                callback();
                return 0;
            });
        }

    public:
        void set(GuiInstance* guiInstance, std::function<void()>& callbackFunc) {
            ALooperMessage::handleMessage = [&]() { handleMessage(); };
            gui_instance = guiInstance;
            callback = callbackFunc;
        }
    };

    void AAPJniFacade::createGuiViaJni(GuiInstance* guiInstance, std::function<void()> callback) {
        NonRealtimeLoopRunner *runner = aap::get_non_rt_loop_runner();
        CreateGuiMessage* msg;
        runner->create<CreateGuiMessage>((void**) &msg);
        msg->set(guiInstance, callback);
        runner->postMessage(msg);
    }

#define GUI_OPCODE_SHOW 1
#define GUI_OPCODE_HIDE 2
#define GUI_OPCODE_DESTROY 4
    class GuiWithViewMessage : public ALooperMessage {
        int32_t opcode;
        GuiInstance* gui_instance;
        std::function<void()> callback;

    protected:
        void handleMessage() {
            usingContext<int32_t>([&](JNIEnv* env, jclass cls, jobject context) {
                if (!gui_instance->view) {
                    // it must be assigned at create() state.
                    gui_instance->lastError = "createGui() was either not invoked or not successful. No further operation is performed.";
                    aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPJniFacade", gui_instance->lastError.c_str());
                    callback();
                    return 0;
                }

                auto pluginIdJString = env->NewStringUTF(gui_instance->pluginId.c_str());
                env->CallStaticVoidMethod(cls,
                                          opcode == GUI_OPCODE_SHOW ? j_method_show_gui :
                                          opcode == GUI_OPCODE_HIDE ? j_method_hide_gui :
                                          j_method_destroy_gui,
                                          context, pluginIdJString, gui_instance->instanceId, (jobject) gui_instance->view);
                auto ex = env->ExceptionOccurred();
                if (ex) {
                    env->ExceptionDescribe();
                    gui_instance->lastError = std::string{"AudioPluginServiceHelper."}
                        + (opcode == GUI_OPCODE_SHOW ? "showGui()" : opcode == GUI_OPCODE_HIDE ? "hideGui()" : "destroyGui()")
                        + "failed. Check the Android log for details.";
                    aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPJniFacade", gui_instance->lastError.c_str());
                    env->ExceptionClear();
                }
                callback();
                return 0;
            });
        }

    public:
        void set(int32_t opCode, GuiInstance* guiInstance, std::function<void()> callbackFunc) {
            ALooperMessage::handleMessage = [&]() { handleMessage(); };
            opcode = opCode;
            gui_instance = guiInstance;
            callback = callbackFunc;
        }
    };

    void processGuiWithViewMessage(int32_t opcode, GuiInstance* guiInstance, std::function<void()> callback) {
        NonRealtimeLoopRunner *runner = aap::get_non_rt_loop_runner();
        GuiWithViewMessage* msg;
        runner->create<GuiWithViewMessage>((void**) &msg);
        msg->set(opcode, guiInstance, callback);
        runner->postMessage(msg);
    }

    void AAPJniFacade::showGuiViaJni(GuiInstance* guiInstance, std::function<void()> callback) {
        processGuiWithViewMessage(GUI_OPCODE_SHOW, guiInstance, callback);
    }

    void AAPJniFacade::hideGuiViaJni(GuiInstance* guiInstance, std::function<void()> callback) {
        processGuiWithViewMessage(GUI_OPCODE_HIDE, guiInstance, callback);
    }

    void AAPJniFacade::destroyGuiViaJni(GuiInstance* guiInstance, std::function<void()> callback) {
        processGuiWithViewMessage(GUI_OPCODE_DESTROY, guiInstance, callback);
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
        assert(classLoaderClass);
        auto findClassMethod = env->GetMethodID(classLoaderClass, "findClass",
                                                "(Ljava/lang/String;)Ljava/lang/Class;");
        CLEAR_JNI_ERROR(env)
        assert(findClassMethod);
        auto settingsClassObj = env->CallObjectMethod(contextClassLoader, findClassMethod,
                                                      env->NewStringUTF(
                                                              "org/androidaudioplugin/hosting/AudioPluginMidiSettings"));
        CLEAR_JNI_ERROR(env)
        assert(settingsClassObj);
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
            assert(klass);
            return env->NewObject(klass, j_method_parameter_ctor, (jint) para->getId(),
                                  env->NewStringUTF(para->getName()), para->getMinimumValue(),
                                  para->getMaximumValue(), para->getDefaultValue());
        });
    }

    jobject AAPJniFacade::getPluginInstancePort(jlong nativeHost, jint instanceId, jint index) {
        return usingJNIEnv<jobject>([&](JNIEnv* env) {
            auto host = (aap::PluginHost *) (void *) nativeHost;
            auto instance = host->getInstanceById(instanceId);
            auto port = instance->getPort(index);
            auto klass = env->FindClass(java_port_information_class_name);
            assert(klass);
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