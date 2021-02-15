#include <cstdlib>
#include <memory>
#include "aap/audio-plugin-host.h"

// it is kind of hack, copying decl. manually.
namespace aaplv2sample {
    int runHostAAP(int sampleRate, const char *pluginID, void *wavL, void *wavR, int wavLength, void *outWavL, void *outWavR, float* parameters);
}

int main(int argc, char** argv)
{
	if (argc < 2) {
		puts ("usage: aaphostsample [plugin-identifier]");
		return 1;
	}

	aap::getPluginHostPAL()->setPluginListCache(aap::getPluginHostPAL()->getInstalledPlugins());

	int sampleRate = 44100;
	const char* pluginID = argv[1];
	auto pal = aap::getPluginHostPAL();
	int size = 1;
	int wavLength = 1000000;
	void* inWavBytesL = calloc(wavLength, 1);
	void* inWavBytesR = calloc(wavLength, 1);
	void* outWavBytesL = calloc(wavLength, 1);
	void* outWavBytesR = calloc(wavLength, 1);
	int ret = aaplv2sample::runHostAAP(sampleRate, pluginID, inWavBytesL, inWavBytesR, wavLength, outWavBytesL, outWavBytesR, nullptr);
	free(inWavBytesL);
	free(inWavBytesR);
	free(outWavBytesL);
	free(outWavBytesR);
}

