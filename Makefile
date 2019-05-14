
ABIS_SIMPLE = x86 x86-64 armv7 arm64

all: build

prepare:
	cd external/cerbero && ANDROID_NDK_HOME=~/android-sdk-linux/ndk-bundle/ ./cerbero-uninstalled -c config/cross-android-x86.cbc bootstrap

.PHONY:
build:
	cd external/cerbero && for abi in $(ABIS_SIMPLE) ; do \
		ANDROID_NDK_HOME=~/android-sdk-linux/ndk-bundle/ ./cerbero-uninstalled -c config/cross-android-$$abi.cbc build mda-lv2 ; \
	done

