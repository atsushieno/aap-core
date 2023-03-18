#!/bin/bash

ANDROID_HOME=~/Android/sdk
BUILD_TOOLS_VERSION=33.0.2
THIS_DIR=`dirname $(readlink -f $0)`
AIDL_DIR=$THIS_DIR
AIDL_TOOL=$ANDROID_HOME/build-tools/$BUILD_TOOLS_VERSION/aidl

$AIDL_TOOL --lang=ndk \
	-o $THIS_DIR/cpp/android/gen \
	-h $THIS_DIR/cpp/android/gen/include \
	-I $THIS_DIR/aidl/ \
	$THIS_DIR/aidl/org/androidaudioplugin/AudioPluginInterface.aidl \
	$THIS_DIR/aidl/org/androidaudioplugin/AudioPluginInterfaceCallback.aidl

