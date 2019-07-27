#!/bin/bash

ANDROID_HOME=~/android-sdk-`uname | sed -e 's/\(.*\)/\L\1/'`
BUILD_TOOLS_VERSION=29.0.0

$ANDROID_HOME/build-tools/$BUILD_TOOLS_VERSION/aidl --lang=ndk -o gen -h gen org/androidaudiopluginframework/AudioPluginInterface.aidl
$ANDROID_HOME/build-tools/$BUILD_TOOLS_VERSION/aidl --lang=java -o gen org/androidaudiopluginframework/AudioPluginInterface.aidl

