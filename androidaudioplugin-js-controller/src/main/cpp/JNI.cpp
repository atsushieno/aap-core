#include <jni.h>
#include <android/log.h>
#include "AapJsControllerRuntime.h"

namespace {

JavaVM* g_vm = nullptr;
jclass g_runtimeClass = nullptr;          // global ref to AapAutomationRuntime
jmethodID g_connectServiceMethod = nullptr;

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

// Cache the class + static method from a JNIEnv that has the app classloader (i.e. one of our JNI
// entry points, which are invoked from the runtime's Java thread). FindClass in JNI_OnLoad uses the
// system classloader and would not find the app class.
void cacheRuntimeRefs(JNIEnv* env) {
    if (g_runtimeClass != nullptr)
        return;
    jclass local = env->FindClass("org/androidaudioplugin/js/AapAutomationRuntime");
    if (local == nullptr) {
        env->ExceptionClear();
        return;
    }
    g_runtimeClass = (jclass) env->NewGlobalRef(local);
    g_connectServiceMethod =
        env->GetStaticMethodID(g_runtimeClass, "connectService", "(Ljava/lang/String;)Z");
    env->DeleteLocalRef(local);
}

} // namespace

namespace aap::js {

// Native -> JVM upcall: ask the host (via AapAutomationRuntime.connectService) to bind a plugin
// service by package name. Returns false if the JVM/refs are unavailable or the bind failed.
// Called on the runtime's single Java thread, so GetEnv yields the attached env.
bool jvmConnectService(const std::string& packageName) {
    if (g_vm == nullptr || g_runtimeClass == nullptr || g_connectServiceMethod == nullptr)
        return false;
    JNIEnv* env = nullptr;
    if (g_vm->GetEnv((void**) &env, JNI_VERSION_1_6) != JNI_OK || env == nullptr)
        return false;
    jstring s = env->NewStringUTF(packageName.c_str());
    jboolean result = env->CallStaticBooleanMethod(g_runtimeClass, g_connectServiceMethod, s);
    env->DeleteLocalRef(s);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return false;
    }
    return result == JNI_TRUE;
}

} // namespace aap::js

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
Java_org_androidaudioplugin_js_AapAutomationRuntime_nativeBootstrap(
        JNIEnv* env, jobject, jstring apiJsSource) {
    cacheRuntimeRefs(env);
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
