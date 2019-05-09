#!/bin/sh

# projucer --resave AndroidAudioPluginFramework.jucer

cd Builds/Android/lib/
mkdir -p build-android
cd build-android
cmake -DCMAKE_TOOLCHAIN_FILE=~/android-sdk-linux/ndk-bundle/build/cmake/android.toolchain.cmake -DANDROID_TOOLCHAIN=clang -DANDROID_PLATFORM=android-16 -DANDROID_STL=c++_static -DANDROID_CPP_FEATURES="exceptions rtti" -DANDROID_ARM_MODE=arm -DANDROID_ARM_NEON=TRUE -DANDROID_ABI=armeabi-v7a -DCMAKE_CXX_FLAGS_DEBUG=-O0 -DCMAKE_C_FLAGS_DEBUG=-O0 -DJUCE_BUILD_CONFIGURATION=DEBUG ..
make

