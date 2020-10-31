#!/bin/bash

# This shell script is to set up ASAN for debugging native libs.
# Specify your own path in those variables below.

ANDROID_NDK_PATH=~/Android/Sdk/ndk/21.3.6528147
HOST_ARCH_LIB=linux-x86_64/lib64
CLANG_VER=9.0.8


CLANG_LIB=$ANDROID_NDK_PATH/toolchains/llvm/prebuilt/$HOST_ARCH_LIB/clang/$CLANG_VER/lib
SAMPLE=java/samples/aaphostsample/src/main
SAMPLE_RES=$SAMPLE/resources/lib

cp $CLANG_LIB/linux/libclang_rt.*-i686-android.so $SAMPLE/jniLibs/x86/
cp $CLANG_LIB/linux/libclang_rt.*-x86_64-android.so $SAMPLE/jniLibs/x86_64/
cp $CLANG_LIB/linux/libclang_rt.*-arm-android.so $SAMPLE/jniLibs/armeabi-v7a/
cp $CLANG_LIB/linux/libclang_rt.*-aarch64-android.so $SAMPLE/jniLibs/arm64-v8a/
mkdir -p $SAMPLE_RES/x86
mkdir -p $SAMPLE_RES/x86_64
mkdir -p $SAMPLE_RES/armeabi-v7a
mkdir -p $SAMPLE_RES/aarch64-v8a
cp -R $ANDROID_NDK_PATH/wrap.sh/asan.sh $SAMPLE_RES/x86/
cp -R $ANDROID_NDK_PATH/wrap.sh/asan.sh $SAMPLE_RES/x86_64/
cp -R $ANDROID_NDK_PATH/wrap.sh/asan.sh $SAMPLE_RES/armeabi-v7a/
cp -R $ANDROID_NDK_PATH/wrap.sh/asan.sh $SAMPLE_RES/aarch64-v8a/
