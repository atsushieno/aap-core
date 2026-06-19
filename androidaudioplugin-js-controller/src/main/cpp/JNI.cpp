#include <jni.h>
#include "AapJsControllerRuntime.h"

namespace {

std::string jstringToStdString(JNIEnv* env, jstring s) {
    if (!s)
        return {};
    const char* chars = env->GetStringUTFChars(s, nullptr);
    std::string result = chars ? chars : "";
    if (chars)
        env->ReleaseStringUTFChars(s, chars);
    return result;
}

aap::js::AapJsControllerRuntime& runtime() {
    return aap::js::AapJsControllerRuntime::global();
}

} // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_androidaudioplugin_js_AapAutomationRuntime_nativeBootstrap(
        JNIEnv* env, jobject, jstring apiJsSource) {
    runtime().bootstrap(jstringToStdString(env, apiJsSource));
}

extern "C" JNIEXPORT void JNICALL
Java_org_androidaudioplugin_js_AapAutomationRuntime_nativeSetClient(
        JNIEnv*, jobject, jlong nativeClientHandle) {
    runtime().setClient((int64_t) nativeClientHandle);
}

extern "C" JNIEXPORT void JNICALL
Java_org_androidaudioplugin_js_AapAutomationRuntime_nativeSetPluginCatalog(
        JNIEnv* env, jobject, jstring json) {
    runtime().setPluginCatalog(jstringToStdString(env, json));
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_androidaudioplugin_js_AapAutomationRuntime_nativeRunScript(
        JNIEnv* env, jobject, jstring code) {
    return env->NewStringUTF(runtime().runScript(jstringToStdString(env, code)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_androidaudioplugin_js_AapAutomationRuntime_nativeStartJob(
        JNIEnv* env, jobject, jstring code) {
    return env->NewStringUTF(runtime().startJob(jstringToStdString(env, code)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_androidaudioplugin_js_AapAutomationRuntime_nativeGetJob(
        JNIEnv* env, jobject, jstring jobId) {
    return env->NewStringUTF(runtime().getJob(jstringToStdString(env, jobId)).c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_org_androidaudioplugin_js_AapAutomationRuntime_nativeClearJob(
        JNIEnv* env, jobject, jstring jobId) {
    runtime().clearJob(jstringToStdString(env, jobId));
}
