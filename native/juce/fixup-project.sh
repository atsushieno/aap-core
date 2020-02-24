#!/bin/sh

rm -f aap_metadata.xml
gcc -g ../../tools/aap-metadata-generator.cpp \
        Builds/LinuxMakefile/build/$APPNAME.a \
        -lstdc++ -ldl -lm -lpthread \
        `pkg-config --libs alsa x11 xinerama xext freetype2 libcurl webkit2gtk-4.0` \
        -o aap-metadata-generator

./aap-metadata-generator `pwd`/aap_metadata.xml

cp ../sample-project.settings.gradle Builds/Android/settings.gradle

# There is no way to generate this in Projucer.
cp ../sample-project.gradle.properties Builds/Android/gradle.properties

# Projucer is too inflexible to generate required content.
cp ../sample-project.build.gradle Builds/Android/build.gradle

mkdir -p Builds/Android/app/src/debug/res/xml/
mkdir -p Builds/Android/app/src/release/res/xml/

cp aap_metadata.xml Builds/Android/app/src/debug/res/xml/
cp aap_metadata.xml Builds/Android/app/src/release/res/xml/

