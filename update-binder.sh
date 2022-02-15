#!/bin/bash

ANDROID_HOME=~/Android/Sdk
BUILD_TOOLS_VERSION=32.0.0
THIS_DIR=`dirname $(readlink -f $0)`
AIDL_DIR=$THIS_DIR
AIDL_TOOL=$ANDROID_HOME/build-tools/$BUILD_TOOLS_VERSION/aidl

$AIDL_TOOL --lang=ndk \
	-o $THIS_DIR/native/androidaudioplugin/android/src/gen \
	-h $THIS_DIR/native/androidaudioplugin/android/src/gen/include \
	androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterface.aidl
