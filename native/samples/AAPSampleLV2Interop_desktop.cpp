#include <cstdlib>
#include <memory>
#include "aap/android-audio-plugin-host.hpp"

// it is kind of hack, copying decl. manually.
namespace aaplv2sample {
    int runHostAAP(int sampleRate, const char *pluginID, void *wavL, void *wavR, int wavLength, void *outWavL, void *outWavR);
}

int main(int argc, char** argv)
{
	int sampleRate = 44100;
	const char* pluginID = argv[1];
	aap::setKnownPluginInfos(aap::aap_parse_plugin_descriptor(argv[2]));
	int size = 1;
	int wavLength = 1000000;
	void* inWavBytesL = calloc(wavLength, 1);
	void* inWavBytesR = calloc(wavLength, 1);
	void* outWavBytesL = calloc(wavLength, 1);
	void* outWavBytesR = calloc(wavLength, 1);
	int ret = aaplv2sample::runHostAAP(sampleRate, pluginID, inWavBytesL, inWavBytesR, wavLength, outWavBytesL, outWavBytesR);
	free(inWavBytesL);
	free(inWavBytesR);
	free(outWavBytesL);
	free(outWavBytesR);
	free(aap::getKnownPluginInfos());
}

