#!/bin/sh

$JUCE_DIR/Projucer --resave android-audio-plugin-framework.jucer

cd Builds/Android/lib/
mkdir -p build-android
cd build-android
cmake -DCMAKE_TOOLCHAIN_FILE=~/Android/Sdk/ndk/20.0.5594570/build/cmake/android.toolchain.cmake -DANDROID_TOOLCHAIN=clang -DANDROID_PLATFORM=android-16 -DANDROID_STL=c++_static -DANDROID_CPP_FEATURES="exceptions rtti" -DANDROID_ARM_MODE=arm -DANDROID_ARM_NEON=TRUE -DANDROID_ABI=armeabi-v7a -DCMAKE_CXX_FLAGS_DEBUG=-O0 -DCMAKE_C_FLAGS_DEBUG=-O0 -DJUCE_BUILD_CONFIGURATION=DEBUG ..
make

