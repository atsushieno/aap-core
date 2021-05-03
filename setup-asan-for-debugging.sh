#!/bin/bash

# This shell script is to set up ASAN for debugging native libs.
# Specify your own path in those variables below.

# They have to be adjusted to your environment and NDK version.
# Although note that whenever we use AAP the NDK version has to be fixed to
# this one otherwise libc++_shared.so version mismatch may bite you.
ANDROID_NDK_PATH=~/Android/Sdk/ndk/21.3.6528147
HOST_ARCH_LIB=linux-x86_64/lib64
CLANG_VER=9.0.8
CLANG_LIB=$ANDROID_NDK_PATH/toolchains/llvm/prebuilt/$HOST_ARCH_LIB/clang/$CLANG_VER/lib

ALL_ABIS=("x86" "x86_64" "armeabi-v7a" "arm64-v8a")

ALL_APPS=("java/samples/aaphostsample" "java/samples/aapbarebonepluginsample")

for sample in "${ALL_APPS[@]}"; do

  SAMPLE=$sample/src/main
  SAMPLE_RES=$SAMPLE/resources/lib

  for a in "${ALL_ABIS[@]}"; do
    echo "ABI: $a"
    mkdir -p $SAMPLE/jniLibs/$a ;
    # This is causing unresponsive debugger. Do not use it. Load asan so at using System.loadLibrary() instead.
    # mkdir -p $SAMPLE_RES/$a
    # cp -R $ANDROID_NDK_PATH/wrap.sh/asan.sh $SAMPLE_RES/$a/wrap.sh
  done

  cp $CLANG_LIB/linux/libclang_rt.asan-i686*.so $SAMPLE/jniLibs/x86
  cp $CLANG_LIB/linux/libclang_rt.asan-x86_64*.so $SAMPLE/jniLibs/x86_64
  cp $CLANG_LIB/linux/libclang_rt.asan-arm*.so $SAMPLE/jniLibs/armeabi-v7a
  cp $CLANG_LIB/linux/libclang_rt.asan-aarch64*.so $SAMPLE/jniLibs/arm64-v8a

done

