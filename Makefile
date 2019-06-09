# This Makefile is to build and copy native LV2 dependencies

ABIS_SIMPLE = x86 x86-64 armv7 arm64
APPNAMES = AAPHostSample AAPLV2Sample

all: build

prepare:
	cd external/cerbero && ANDROID_NDK_HOME=~/android-sdk-linux/ndk-bundle/ ./cerbero-uninstalled -c config/cross-android-x86.cbc bootstrap

.PHONY:
build: copy-lv2-deps

.PHONY:
copy-lv2-deps: build-lv2-deps
	make A_ARCH=x86 A_ARCH2=x86 copy-lv2-deps-single
	make A_ARCH=x86_64 A_ARCH2=x86_64 copy-lv2-deps-single
	make A_ARCH=armv7 A_ARCH2=armeabi-v7a copy-lv2-deps-single
	make A_ARCH=arm64 A_ARCH2=arm64-v8a copy-lv2-deps-single

copy-lv2-deps-single:
	for appname in $(APPNAMES) ; do \
		mkdir -p poc-samples/$$appname/app/src/main/jniLibs/$(A_ARCH2) && \
		cp external/cerbero/build/dist/android_$(A_ARCH)/lib/*.so poc-samples/$$appname/app/src/main/jniLibs/$(A_ARCH2)/ && \
		cp -R external/cerbero/build/dist/android_$(A_ARCH)/lib/lv2 poc-samples/$$appname/app/src/main/jniLibs/$(A_ARCH2)/ ; \
	done

.PHONY:
build-lv2-deps:
	cd external/cerbero && for abi in $(ABIS_SIMPLE) ; do \
		ANDROID_NDK_HOME=~/android-sdk-linux/ndk-bundle/ ./cerbero-uninstalled -c config/cross-android-$$abi.cbc build mda-lv2 ; \
	done

