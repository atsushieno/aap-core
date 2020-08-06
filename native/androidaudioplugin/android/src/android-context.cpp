#include "aap/android-context.h"

namespace aap {

// Android-specific API. Not sure if we would like to keep it in the host API - it is for plugins.
JavaVM *android_vm{nullptr};
jobject application_context{nullptr};

void set_application_context(JNIEnv *env, jobject jobjectApplicationContext) {
    if (application_context)
        env->DeleteGlobalRef(application_context);
	env->GetJavaVM(&android_vm);
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

// FIXME: it is kind of hack; dynamically loaded and used by JuceAAPWrapper.
extern "C" JavaVM *getJVM() { return aap::android_vm; }
