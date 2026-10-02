#include <jni.h>
// The connector's native notification is outside these lifecycle tests. Binder registration
// is separately observed by the Kotlin platform doubles; no production JNI library is loaded.
JNIEXPORT void JNICALL
Java_org_androidaudioplugin_hosting_AudioPluginServiceConnector_nativeOnServiceConnectedCallback(
        JNIEnv* env, jobject connector, jstring package_name) {}
