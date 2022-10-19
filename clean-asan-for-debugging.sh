#!/bin/bash

# This shell script is to set up ASAN for debugging native libs.
# Specify your own path in those variables below.

# They have to be adjusted to your environment and NDK version.
# Although note that whenever we use AAP the NDK version has to be fixed to
# this one otherwise libc++_shared.so version mismatch may bite you.
#
# Also note that you need below in build.gradle:
# > android.packagingOptions.jniLibs.useLegacyPackaging = true
#
ANDROID_NDK_PATH=~/Android/Sdk/ndk/21.4.7075529
#ANDROID_NDK_PATH=~/Android/Sdk/ndk/23.1.7779620
CLANG_VER=9.0.9
#CLANG_VER=12.0.8
HOST_ARCH_LIB=linux-x86_64/lib64
CLANG_LIB=$ANDROID_NDK_PATH/toolchains/llvm/prebuilt/$HOST_ARCH_LIB/clang/$CLANG_VER/lib

ALL_ABIS=("x86" "x86_64" "armeabi-v7a" "arm64-v8a")

ALL_APPS=("samples/aaphostsample2" "samples/aaphostsample" "samples/aapbarebonepluginsample" "samples/aapinstrumentsample")

for sample in "${ALL_APPS[@]}"; do

  echo "APP: $sample"
  SAMPLE=$sample/src/main
  SAMPLE_RES=$SAMPLE/resources/lib

  for a in "${ALL_ABIS[@]}"; do
    echo "  ABI: $a"
    rm $SAMPLE_RES/$a/wrap.sh
  done

  rm $SAMPLE/jniLibs/*/libclang_rt.asan-*.so

done

echo "done."
echo "NOTE: do not forget to reset "enable_asan" in the root build.gradle.kts."
