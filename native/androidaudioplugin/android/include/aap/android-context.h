#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/binder_ibinder.h>

namespace aap {

void set_application_context(JNIEnv *env, jobject applicationContext);

JavaVM *get_android_jvm();

jobject get_android_application_context();

AAssetManager *get_android_asset_manager(JNIEnv* env);
}
