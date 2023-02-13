#ifndef ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_APPLICATION_CONTEXT_H
#define ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_APPLICATION_CONTEXT_H

#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/binder_ibinder.h>

namespace aap {

/**
 * It is a utility function that is supposed to be called at androidx.startup:startup-runtime
 * (or plain old ContentProvider#onCreate()) to ensure that app context can be accessible
 * by further native code.
 *
 * @param env JNIEnv that is passed by androidx.startup.Initializer#create() overrides.
 * @param applicationContext the app context to store. We will add GlobalRef.
 */
void set_application_context(JNIEnv *env, jobject applicationContext);
/**
 * (Currently not in use as there is no "always graceful" app quit on Android platform.)
 * @param env
 */
void unset_application_context(JNIEnv *env);

JavaVM *get_android_jvm();

/**
 * Returns the android application context that was set by set_application_context().
 * @return the application context which was set in prior, or nullptr if it was nto set.
 */
jobject get_android_application_context();

/**
 * Returns the asset manager that is bound to the application context. It is instantiated via
 * getAssetManager() through JNI, created a global ref and then returned.
 * @param env
 * @return the asset manager, of AAssetManager type.
 */
AAssetManager *get_android_asset_manager(JNIEnv* env);

}

#endif /* ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_APPLICATION_CONTEXT_H */
