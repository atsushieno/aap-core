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
if [ "$ANDROID_NDK_PATH" == "" ]; then
  if [ `uname` == "Darwin" ]; then
    ANDROID_NDK_PATH=~/Library/Android/Sdk/ndk/25.1.8937393
  else
    ANDROID_NDK_PATH=~/Android/Sdk/ndk/25.1.8937393
  fi
fi
UNAMEXX=`uname | tr '[:upper:]' '[:lower:]'`
echo "UNAME: $UNAMEXX"
HOST_ARCH_LIB=$UNAMEXX-x86_64/lib64
CLANG_VER=14.0.6
CLANG_LIB=$ANDROID_NDK_PATH/toolchains/llvm/prebuilt/$HOST_ARCH_LIB/clang/$CLANG_VER/lib

ALL_ABIS=("x86" "x86_64" "armeabi-v7a" "arm64-v8a")

ALL_APPS=("samples/aaphostsample" "samples/aapbarebonepluginsample" "samples/aapinstrumentsample" "samples/aapxssample")

for sample in "${ALL_APPS[@]}"; do
  echo "APP: $sample"

  SAMPLE=$sample/src/main
  SAMPLE_RES=$SAMPLE/resources/lib

  for a in "${ALL_ABIS[@]}"; do
    echo "  ABI: $a"
    mkdir -p $SAMPLE/jniLibs/$a ;
    # This is causing unresponsive debugger. Do not use it. Load asan so at using System.loadLibrary() instead.
    mkdir -p $SAMPLE_RES/$a
    cp -R asan-wrap-bugfixed.sh $SAMPLE_RES/$a/wrap.sh
    dos2unix $SAMPLE_RES/$a/wrap.sh
    chmod +x $SAMPLE_RES/$a/wrap.sh
  done

  chmod +x $CLANG_LIB/linux/libclang_rt.asan-*.so
  cp $CLANG_LIB/linux/libclang_rt.asan-i686*.so $SAMPLE/jniLibs/x86
  cp $CLANG_LIB/linux/libclang_rt.asan-x86_64*.so $SAMPLE/jniLibs/x86_64
  cp $CLANG_LIB/linux/libclang_rt.asan-arm*.so $SAMPLE/jniLibs/armeabi-v7a
  cp $CLANG_LIB/linux/libclang_rt.asan-aarch64*.so $SAMPLE/jniLibs/arm64-v8a

done

echo "done."
echo "NOTE: do not forget to set "enable_asan" in the root build.gradle.kts."

