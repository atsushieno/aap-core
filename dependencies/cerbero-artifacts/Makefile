# This Makefile is to build and copy native LV2 dependencies

PWD=$(shell pwd)

ABIS_SIMPLE = x86 x86-64 armv7 arm64
ANDROID_NDK=/home/$(USER)/Android/Sdk/ndk/20.0.5594570/

TOP=`pwd`

all: build add-symlinks

build: build-native

.PHONY:
clean:
	make CERBERO_COMMAND=wipe run-cerbero-command

.PHONY:
prepare:
	cd cerbero && ANDROID_NDK_HOME=$(ANDROID_NDK) ./cerbero-uninstalled -c config/cross-android-x86.cbc bootstrap


.PHONY:
build-native: build-lv2-deps


.PHONY:
add-symlinks:
	rm -rf outputs
	mkdir -p outputs/
	if [ ! -e symlinked/x86 ] ; then ln -s $(TOP)/cerbero/build/dist/android_x86/ outputs/x86 ; fi
	if [ ! -e symlinked/x86_64 ] ; then ln -s $(TOP)/cerbero/build/dist/android_x86_64/ outputs/x86_64 ; fi
	if [ ! -e symlinked/armeabi-v7a ] ; then ln -s $(TOP)/cerbero/build/dist/android_armv7/ outputs/armeabi-v7a ; fi
	if [ ! -e symlinked/arm64-v8a ] ; then ln -s $(TOP)/cerbero/build/dist/android_arm64/ outputs/arm64-v8a ; fi


.PHONY:
build-lv2-deps:
	make CERBERO_COMMAND="build libsndfile" run-cerbero-command
	# make CERBERO_COMMAND="build serd sord" run-cerbero-command
	# make CERBERO_COMMAND="build lv2 sratom lilv mda-lv2" run-cerbero-command


.PHONY:
run-cerbero-command:
	cd cerbero && for abi in $(ABIS_SIMPLE) ; do \
		ANDROID_NDK_HOME=$(ANDROID_NDK) BUILD_ANDROID_ABI=$$abi ./cerbero-uninstalled -c config/cross-android-$$abi.cbc $(CERBERO_COMMAND) ; \
	done

