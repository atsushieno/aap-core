#!/bin/sh

rm -f aap_metadata.xml
gcc -g ../../tools/aap-metadata-generator.cpp \
        Builds/LinuxMakefile/build/$APPNAME.a \
        -lstdc++ -ldl -lm -lpthread \
        `pkg-config --libs alsa x11 xinerama xext freetype2 libcurl webkit2gtk-4.0` \
        -o aap-metadata-generator

./aap-metadata-generator `pwd`/aap_metadata.xml

cp ../../sample-project-settings.gradle Builds/Android/settings.gradle

mkdir -p Builds/Android/androidaudioplugin-debug/
cp ../../../../java/androidaudioplugin/build/outputs/aar/androidaudioplugin-debug.aar Builds/Android/androidaudioplugin-debug/
cp ../../androidaudioplugin-debug-build.gradle Builds/Android/androidaudioplugin-debug/build.gradle

mkdir -p Builds/Android/app/src/debug/res/xml/
mkdir -p Builds/Android/app/src/release/res/xml/

cp aap_metadata.xml Builds/Android/app/src/debug/res/xml/
cp aap_metadata.xml Builds/Android/app/src/release/res/xml/

