#!/bin/sh

CURDIR="$( cd `dirname $0` >/dev/null 2>&1 && pwd )"

echo "Entering $CURDIR ..."
cd $CURDIR

$JUCE_DIR/Projucer --resave android-audio-plugin-framework.jucer

# There is no way to generate those files in Projucer.
cp project-override.build.gradle Builds/Android/build.gradle
cp copy-into-generated-project.gradle.properties Builds/Android/gradle.properties

sed -e "s/project (\"JuceAAPAudioPluginHost\" C CXX)/project (\"JuceAAPAudioPluginHost\" C CXX)\n\nlink_directories (\n  \$\{CMAKE_CURRENT_SOURCE_DIR\}\/..\/..\/..\/..\/build\/native\/androidaudioplugin \n  \$\{CMAKE_CURRENT_SOURCE_DIR\}\/..\/..\/..\/..\/build\/native\/androidaudioplugin-lv2) \n\n/" Builds/CLion/CMakeLists.txt  > Builds/CLion/CMakeLists.txt.patched
mv Builds/CLion/CMakeLists.txt.patched Builds/CLion/CMakeLists.txt

cd Builds/LinuxMakefile && make && cd ../..

cd Builds/Android && ./gradlew -d build && cd ../..

