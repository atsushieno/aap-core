# AAP JUCE audio processor and client modules

This is a place where we have JUCE modules for AAP, both plugins and hosts.

- Host sample
  - JuceAAPAudioPluginHost
- Plugin sample
  - andes

## Build Instruction

This is what @atsushieno usually runs:

```
$ JUCE_DIR=/path/to/JUCE_sdk ./samples/JuceAAPAudioPluginHost/build.sh
$ JUCE_DIR=/path/to/JUCE_sdk ./samples/andes/build.sh
```

Depending on the NDK setup you might also have to rewrite `Builds/Android/local.properties` to point to the right NDK location. Then run `cd Builds/Android && ./gradlew build` instead of `./build.sh`.

You have to build `androidaudioplugin` from the top directory first.
Otherwise cmake will fail to resolve it and result in NOTFOUND.


## Generating aap_metadata.xml

To import JUCE audio plugins into AAP world, we have to create plugin
descriptor (`aap_metadata.xml`). We have a supplemental tool source to
help generating it automatically from JUCE plugin binary (shared code).

The command line below builds `aap-metadata-generator` under
`Builds/LinuxMakefile/build` and generates `aap_metadata.xml` at the plugin
top level directory:

```
APP=__yourapp__ gcc (__thisdir__)/tools/aap-metadata-generator.cpp \
	Builds/LinuxMakefile/build/$APP.a \
	-lstdc++ -ldl -lm -lpthread \
	`pkg-config --libs alsa x11 xinerama xext freetype2 libcurl` \
	-o aap-metadata-generator \
	&&./aap-metadata-generator aap_metadata.xml
```


## Code origin and license

The sample app code under `samples/JuceAAPAudioPluginHost/Source` directory is slightly modified version of
JUCE [AudioPluginHost](https://github.com/WeAreROLI/JUCE/tree/master/extras/AudioPluginHost)
which is distributed under the GPLv3 license.

The sample app code under `samples/SARAH` directory is slightly modified version of [SARAH](https://github.com/getdunne/SARAH) which is distributed under the MIT license (apart from JUCE under GPLv3).

