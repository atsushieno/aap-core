#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <unistd.h>
#include <dlfcn.h>
#include <cmath>
#include <cstring>
#include <vector>
#include "aap/android-audio-plugin-host.hpp"

extern "C" {

char *_aap_lv2_path = nullptr;
char *aap_android_get_lv2_path() { return _aap_lv2_path; }
void aap_android_set_lv2_path(char *path) { _aap_lv2_path = strdup(path); }
void aap_android_release_lv2_path() { free(_aap_lv2_path); }

}


namespace aaplv2 {

    typedef void (*set_io_context_func)(void *);

    set_io_context_func libserd_set_context = NULL;
    set_io_context_func liblilv_set_context = NULL;

    void ensureDLEach(const char *libname, set_io_context_func &context) {
        if (context == NULL) {
            auto lib = dlopen(libname, RTLD_NOW);
            assert (lib != NULL);
            context = (set_io_context_func) dlsym(lib, "abstract_set_io_context");
            assert (context != NULL);
        }
    }

    void ensureDLLoaded() {
        ensureDLEach("libserd-0.so", libserd_set_context);
        ensureDLEach("liblilv-0.so", liblilv_set_context);
    }

    void set_io_context(AAssetManager *am) {
        ensureDLLoaded();
        libserd_set_context(am);
        liblilv_set_context(am);
    }

    void cleanup ()
    {
        if (aap_android_get_lv2_path())
            aap_android_release_lv2_path();
        set_io_context(NULL);
    }
}


extern "C" {

void Java_org_androidaudioplugin_lv2_AudioPluginLV2LocalHost_initialize(JNIEnv *env, jclass cls,
                                                                        jobject lv2PathString,
                                                                        jobject assets) {
    aaplv2::set_io_context(AAssetManager_fromJava(env, assets));

    jboolean isCopy = JNI_TRUE;
    auto s = env->GetStringUTFChars((jstring) lv2PathString, &isCopy);
    s = isCopy ? s : strdup(s);
    aap_android_set_lv2_path((char *) s);
}

void Java_org_androidaudioplugin_lv2_AudioPluginLV2LocalHost_cleanup(JNIEnv *env, jclass cls) {
    aaplv2::cleanup();
    aap_android_release_lv2_path();
}

}
