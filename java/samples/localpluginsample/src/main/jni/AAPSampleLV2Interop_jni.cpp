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

namespace aaplv2sample {
    int runHostAAP(int sampleRate, const char **pluginIDs, int numPluginIDs, void *wavL, void *wavR, int wavLength, void *outWavL, void *outWavR);
}

extern "C" {

jint Java_org_androidaudioplugin_localpluginsample_AAPSampleLocalInterop_runHostAAP(JNIEnv *env, jclass cls, jobjectArray jPlugins, jint sampleRate, jbyteArray inWavL, jbyteArray inWavR, jbyteArray outWavL, jbyteArray outWavR)
{
    jsize size = env->GetArrayLength(jPlugins);
    const char *pluginIDs[size];
    for (int i = 0; i < size; i++) {
        auto strUriObj = (jstring) env->GetObjectArrayElement(jPlugins, i);
        jboolean isCopy;
        const char *s = env->GetStringUTFChars(strUriObj, &isCopy);
        pluginIDs[i] = strdup(s);
        env->ReleaseStringUTFChars(strUriObj, s);
    }

    int wavLength = env->GetArrayLength(inWavL);

    void* wavBytesL = calloc(wavLength, 1);
    env->GetByteArrayRegion(inWavL, 0, wavLength, (jbyte*) wavBytesL);
	void* wavBytesR = calloc(wavLength, 1);
	env->GetByteArrayRegion(inWavL, 0, wavLength, (jbyte*) wavBytesR);
    void* outWavBytesL = calloc(wavLength, 1);
	void* outWavBytesR = calloc(wavLength, 1);

    int ret = aaplv2sample::runHostAAP(sampleRate, pluginIDs, size, wavBytesL, wavBytesR, wavLength, outWavBytesL, outWavBytesR);

    env->SetByteArrayRegion(outWavL, 0, wavLength, (jbyte*) outWavBytesL);
	env->SetByteArrayRegion(outWavR, 0, wavLength, (jbyte*) outWavBytesR);

    for(int i = 0; i < size; i++)
        free((char*) pluginIDs[i]);
    free(wavBytesL);
	free(wavBytesR);
	free(outWavBytesR);
    free(outWavBytesL);
    return ret;
}

}

