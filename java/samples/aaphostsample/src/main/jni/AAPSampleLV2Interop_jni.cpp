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

// FIXME: sort out final library header structures.
//   This should be under Platform Abstraction Layer (i.e. hide Android specifics).
namespace aap {
    extern aap::PluginInformation *pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation);
    extern const char *strdup_fromJava(JNIEnv *env, jstring s);
}
namespace aaplv2sample {
    int runHostAAP(int sampleRate, const char **pluginIDs, int numPluginIDs, void *wav, int wavLength, void *outWav);
}

extern "C" {

jint Java_org_androidaudioplugin_aaphostsample_AAPSampleLV2Interop_runHostAAP(JNIEnv *env, jclass cls, jobjectArray jPlugins, jint sampleRate, jbyteArray wav, jbyteArray outWav)
{
    jsize size = env->GetArrayLength(jPlugins);
    const char *pluginIDs[size];
    for (int i = 0; i < size; i++) {
        auto strUriObj = (jstring) env->GetObjectArrayElement(jPlugins, i);
        pluginIDs[i] = aap::strdup_fromJava(env, strUriObj);
    }

    int wavLength = env->GetArrayLength(wav);
    void* wavBytes = calloc(wavLength, 1);
    env->GetByteArrayRegion(wav, 0, wavLength, (jbyte*) wavBytes);
    void* outWavBytes = calloc(wavLength, 1);

    int ret = aaplv2sample::runHostAAP(sampleRate, pluginIDs, size, wavBytes, wavLength, outWavBytes);

    env->SetByteArrayRegion(outWav, 0, wavLength, (jbyte*) outWavBytes);

    for(int i = 0; i < size; i++)
        free((char*) pluginIDs[i]);
    free(wavBytes);
    free(outWavBytes);
    return ret;
}

}

