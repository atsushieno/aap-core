#!/bin/bash

ANDROID_NDK_PATH=/opt/android-ndk
ABIS_SIMPLE=x86 x86-64 armeabi armv8

sudo apt-get update
echo y | sudo apt-get install  autotools-dev automake autoconf libtool g++ autopoint make cmake \
            bison flex yasm pkg-config gtk-doc-tools libxv-dev libx11-dev libpulse-dev \
            python3-dev texinfo gettext build-essential pkg-config doxygen curl libxext-dev \
            libxi-dev x11proto-record-dev libxrender-dev libgl1-mesa-dev libxfixes-dev \
            libxdamage-dev libxcomposite-dev libasound2-dev libxml-simple-perl dpkg-dev \
            debhelper build-essential devscripts fakeroot transfig gperf libdbus-glib-1-dev \
            wget glib-networking libxtst-dev libxrandr-dev libglu1-mesa-dev libegl1-mesa-dev \
            git subversion xutils-dev intltool ccache python3-setuptools \
            autogen meson

ln -s /opt/android-sdk-linux ~/android-sdk-linux
ln -s /opt/android-ndk ~/android-sdk-linux/ndk-bundle

cd git clone https://github.com/Kitware/CMake.git --depth 1 -b v3.14.3 cmake
cd cmake && mkdir build && cd build && cmake .. && make && cd ../..

PATH=`pwd`/cmake/build/bin:$PATH

cd cerbero && ./cerbero-uninstalled -c config/cross-android-x86.cbc bootstrap && cd ..

cd cerbero && for abi in $ABIS_SIMPLE ; do \
		ANDROID_NDK_HOME=$(ANDROID_NDK) ./cerbero-uninstalled -c config/cross-android-$$abi.cbc build lilv mda-lv2 ; \
	done && cd ..

