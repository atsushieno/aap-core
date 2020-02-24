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
    int runHostAAP(int sampleRate, const char *pluginID, void *wavL, void *wavR, int wavLength, void *outWavL, void *outWavR, float *parameters);
}

extern "C" {

jint Java_org_androidaudioplugin_localpluginsample_AAPSampleLocalInterop_runHostAAP(JNIEnv *env, jclass cls, jobjectArray jPlugins, jint sampleRate, jbyteArray inWavL, jbyteArray inWavR, jbyteArray outWavL, jbyteArray outWavR, jfloatArray jParameters)
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
	int numParameters = env->GetArrayLength(jParameters);
	float *parameters = (float*) calloc(numParameters, sizeof(float));
	assert(parameters != nullptr);
	env->GetFloatArrayRegion(jParameters, 0, numParameters, parameters);

    int ret = aaplv2sample::runHostAAP(sampleRate, pluginIDs[0], wavBytesL, wavBytesR, wavLength, outWavBytesL, outWavBytesR, parameters);

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

