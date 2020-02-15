#!/bin/sh

rm aap_metadata.xml
APP=Andes_1 gcc ../../tools/aap-metadata-generator.cpp \
        Builds/LinuxMakefile/build/$APP.a \
        -lstdc++ -ldl -lm -lpthread \
        `pkg-config --libs alsa x11 xinerama xext freetype2 libcurl` \
        -o aap-metadata-generator \
        &&./aap-metadata-generator aap_metadata.xml

cp project-settings.gradle Builds/Android/settings.gradle

mkdir -p Builds/Android/androidaudioplugin-debug/
cp ../../../../java/androidaudioplugin/build/outputs/aar/androidaudioplugin-debug.aar Builds/Android/androidaudioplugin-debug/
cp androidaudioplugin-debug-build.gradle Builds/Android/androidaudioplugin-debug/build.gradle

mkdir -p Builds/Android/app/src/debug/res/xml/
mkdir -p Builds/Android/app/src/release/res/xml/

cp aap_metadata.xml Builds/Android/app/src/debug/res/xml/
cp aap_metadata.xml Builds/Android/app/src/release/res/xml/
cp androidaudioplugin-debug-build.gradle Builds/Android/androidaudioplugin-debug/build.gradle

