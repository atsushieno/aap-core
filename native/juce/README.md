# AAP JUCE audio processor and client modules

This is a place where we have JUCE modules for AAP, both plugins and hosts.

## Build Instruction

This is what @atsushieno usually runs:

```
$ JUCE_DIR=/path/to/JUCE_sdk ./build.sh
```

Depending on the NDK setup you might also have to rewrite `Builds/Android/local.properties` to point to the right NDK location. Then run `cd Builds/Android && ./gradlew build` instead of `./build.sh`.

You have to build `androidaudioplugin` from the top directory first.
Otherwise cmake will fail to resolve it and result in NOTFOUND.

## Code origin and license

The sample app code under `Source` directory is slightly modified version of
JUCE [AudioPluginHost](https://github.com/WeAreROLI/JUCE/tree/master/extras/AudioPluginHost)
which is distributed under the GPLv3 license.

