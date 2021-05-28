#include "aap/android-application-context.h"

namespace aap {

// Android-specific API. Not sure if we would like to keep it in the host API - it is for plugins.
JavaVM *android_vm{nullptr};
jobject application_context{nullptr};

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
	android_vm = vm;
	return JNI_VERSION_1_6;
}

void unset_application_context(JNIEnv *env) {
	if (application_context)
		env->DeleteGlobalRef(application_context);
}

void set_application_context(JNIEnv *env, jobject jobjectApplicationContext) {
    if (application_context)
        unset_application_context(env);
	application_context = env->NewGlobalRef((jobject) jobjectApplicationContext);
}

JavaVM *get_android_jvm() { return android_vm; }

jobject get_android_application_context() { return application_context; }

AAssetManager *get_android_asset_manager(JNIEnv* env) {
	if (!application_context)
		return nullptr;
	auto appClass = env->GetObjectClass(application_context);
	auto getAssetsID = env->GetMethodID(appClass, "getAssets", "()Landroid/content/res/AssetManager;");
	auto assetManagerJ = env->CallObjectMethod(application_context, getAssetsID);
	return AAssetManager_fromJava(env, assetManagerJ);
}

}
