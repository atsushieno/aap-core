# This Makefile is to build and copy native LV2 dependencies

PWD=$(shell pwd)

ABIS_SIMPLE = x86 x86-64 armv7 arm64
ANDROID_NDK=/home/$(USER)/Android/Sdk/ndk/20.0.5594570/

TOP=`pwd`

all: build copy-outputs

build: build-native

.PHONY:
clean:
	make CERBERO_COMMAND=wipe run-cerbero-command

.PHONY:
prepare:
	cd cerbero && ANDROID_NDK_HOME=$(ANDROID_NDK) ./cerbero-uninstalled -c config/cross-android-x86.cbc bootstrap

.PHONY:
copy-outputs:
	mkdir -p ../dist/x86
	mkdir -p ../dist/x86_64
	mkdir -p ../dist/armeabi-v7a
	mkdir -p ../dist/arm64-v8a
	cp -uR $(TOP)/cerbero/build/dist/android_x86/* ../dist/x86/
	cp -uR $(TOP)/cerbero/build/dist/android_x86_64/* ../dist/x86_64/
	cp -uR $(TOP)/cerbero/build/dist/android_armv7/* ../dist/armeabi-v7a/
	cp -uR $(TOP)/cerbero/build/dist/android_arm64/* ../dist/arm64-v8a/

.PHONY:
build-native:
	make CERBERO_COMMAND="build libsndfile" run-cerbero-command

.PHONY:
run-cerbero-command:
	cd cerbero && for abi in $(ABIS_SIMPLE) ; do \
		ANDROID_NDK_HOME=$(ANDROID_NDK) BUILD_ANDROID_ABI=$$abi ./cerbero-uninstalled -c config/cross-android-$$abi.cbc $(CERBERO_COMMAND) ; \
	done

