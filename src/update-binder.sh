#!/bin/bash

~/android-sdk-`uname | sed -e 's/\(.*\)/\L\1/'`/build-tools/29.0.0/aidl --lang=ndk -o gen -h gen aap/IAudioPluginService.aidl
