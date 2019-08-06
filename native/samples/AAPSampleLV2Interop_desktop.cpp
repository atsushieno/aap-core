#include <memory>

// it is kind of hack, copying decl. manually.
namespace aaplv2sample {
    int runHostAAP(int sampleRate, const char **pluginIDs, int numPluginIDs, void *wav, int wavLength, void *outWav);
}

int main(int argc, char** argv)
{
	int sampleRate = 44100;
	const char* pluginIDs[1]{argv[1]};
	int size = 1;
	int wavLength = 1000000;
	void* inWavBytes = calloc(wavLength, 1);
	void* outWavBytes = calloc(wavLength, 1);
	int ret = aaplv2sample::runHostAAP(sampleRate, pluginIDs, size, inWavBytes, wavLength, outWavBytes);
}

