
MINIMIZE_INTERMEDIATES=0
ABIS_SIMPLE= x86 x86_64 armeabi-v7a arm64-v8a
ANDROID_NDK=$(ANDROID_SDK_ROOT)/ndk/21.0.6113669

NATIVE_BINARIES_TAG=r1

all: build-all

build-all: \
	maybe-download-ndk \
	get-lv2-deps \
	build-desktop \
	import-lv2-deps \
	build-java

.PHONY:
maybe-download-ndk: $(ANDROID_NDK)

$(ANDROID_NDK):
	if ! [ -d "$(ANDROID_SDK_ROOT)" ] ; then \
		echo "ANDROID_SDK_ROOT is not defined." ; \
		exit 1 ; \
	fi
	echo "Android NDK will be installed as $(ANDROID_NDK)"
	wget https://dl.google.com/android/repository/android-ndk-r21-linux-x86_64.zip >/dev/null
	unzip android-ndk-r21-linux-x86_64.zip >/dev/null
	mkdir -p $(ANDROID_NDK)
	mv android-ndk-r21/* $(ANDROID_NDK)
	if [ $MINIMIZE_INTERMEDIATES ] ; then \
		rm android-ndk-r21-linux-x86_64.zip ; \
	fi

get-lv2-deps: dependencies/dist/stamp

dependencies/dist/stamp: android-lv2-binaries.zip
	mkdir -p dependencies
	unzip android-lv2-binaries -d dependencies
	touch dependencies/dist/stamp

android-lv2-binaries.zip:
	wget https://github.com/atsushieno/android-native-audio-builders/releases/download/refs/heads/$(NATIVE_BINARIES_TAG)/android-lv2-binaries.zip

build-desktop:
	echo TODO: covers core and lv2 so far. Need to build for testing on desktop
	export PKG_CONFIG_PATH=../lv2-desktop/dist/lib/pkgconfig && \
	cd dependencies && \
		cd serd && ./waf -d --no-utils --prefix=../lv2-desktop/dist configure build install && cd .. && \
		cd sord && ./waf -d --no-utils --prefix=../lv2-desktop/dist configure build install && cd .. && \
		cd lv2 && ./waf -d --prefix=../lv2-desktop/dist configure build install && cd .. && \
		cd sratom && ./waf -d --prefix=../lv2-desktop/dist configure build install && cd .. && \
		cd lilv && ./waf -d --no-utils --prefix=../lv2-desktop/dist configure build install && cd .. && \
		cd mda-lv2 && ./waf -d --prefix=../lv2-desktop/dist configure build install && cd .. && \
	cd ..
	mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make
	cd docs && doxygen && cd ..

build-android:
	for abi in $(ABIS_SIMPLE) ; do \
		make A_ABI=$$abi build-android-single ; \
	done

build-android-single:
	mkdir -p build-android/$(A_ABI) && cd build-android/$(A_ABI) && \
	cmake -DCMAKE_BUILD_TYPE=Debug \
		-DANDROID_STL=c++_shared -DBUILD_SHARED_LIBS=on \
		-DCMAKE_TOOLCHAIN_FILE=$(ANDROID_NDK)/build/cmake/android.toolchain.cmake \
		-DANDROID_ABI=$(A_ABI) -DCMAKE_ANDROID_ARCH_ABI=$(A_ABI) \
		-DANDROID_PLATFORM=android-29 ../.. && make

import-lv2-deps: build-lv2-importer
	mkdir -p java/samples/aaphostsample/src/main/res/xml
	bash import-lv2-deps.sh

build-lv2-importer:
	cd tools/aap-import-lv2-metadata && rm -rf build && mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make

build-java:
	cd java && ANDROID_SDK_ROOT=$(ANDROID_SDK_ROOT) ./gradlew build dokka

