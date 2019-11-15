#!/bin/bash

ABIS_SIMPLE=(x86 x86_64 armeabi-v7a arm64-v8a)

# Remove existing jniLibs for sanity
rm -rf java/androidaudioplugin-lv2/src/main/jniLibs/
rm -rf java/samples/aaphostsample/src/main/jniLibs/
rm -rf java/samples/localpluginsample/src/main/jniLibs/


for abi in ${ABIS_SIMPLE[*]}
do
    echo "ABI: $abi"
    # Copy native libs for each ABI
    mkdir -p java/androidaudioplugin-lv2/src/main/jniLibs/$abi
    cp -R dependencies/cerbero-artifacts/outputs/$abi/lib/*.so java/androidaudioplugin-lv2/src/main/jniLibs/$abi/
    # And then copy native libs of LV2 plugins for each ABI.
    mkdir -p java/samples/aaphostsample/src/main/jniLibs/$abi
    mkdir -p java/samples/localpluginsample/src/main/jniLibs/$abi
    cp -R dependencies/cerbero-artifacts/outputs/$abi/lib/lv2/*/*.so java/samples/aaphostsample/src/main/jniLibs/$abi/
    cp -R dependencies/cerbero-artifacts/outputs/$abi/lib/lv2/*/*.so java/samples/localpluginsample/src/main/jniLibs/$abi/
done

# Copy LV2 metadata files etc.
# The non-native parts should be the same so we just copy files from x86 build.
cp -R dependencies/cerbero-artifacts/outputs/x86/lib/lv2/ java/samples/aaphostsample/src/main/assets/
cp -R dependencies/cerbero-artifacts/outputs/x86/lib/lv2/ java/samples/localpluginsample/src/main/assets/
# ... except for *.so files. They are stored under jniLibs.
rm java/samples/aaphostsample/src/main/assets/lv2/*/*.so
rm java/samples/localpluginsample/src/main/assets/lv2/*/*.so

# Generate `aap-metadata.xml` that AAP service look up plugins.
tools/aap-import-lv2-metadata/build/aap-import-lv2-metadata \
	java/samples/aaphostsample/src/main/assets/lv2 \
	java/samples/aaphostsample/src/main/res/xml
tools/aap-import-lv2-metadata/build/aap-import-lv2-metadata \
	java/samples/localpluginsample/src/main/assets/lv2 \
	java/samples/localpluginsample/src/main/res/xml

