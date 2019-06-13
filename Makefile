# This Makefile is to build and copy native LV2 dependencies

ABIS_SIMPLE = x86 x86-64 armv7 arm64
APPNAMES = AAPHostSample AAPLV2Sample
ANDROID_NDK=~/android-sdk-`uname`/ndk-bundle

TOP=`pwd`

all: build

.PHONY:
clean:
		make CERBERO_COMMAND=wipe run-cerbero-command

.PHONY:
prepare:
	cd external/cerbero && ANDROID_NDK_HOME=$(ANDROID_NDK) ./cerbero-uninstalled -c config/cross-android-x86.cbc bootstrap


.PHONY:
build: build-lv2-deps copy-lv2-deps add-symlinks


.PHONY:
add-symlinks:
	rm -r symlinked-dist
	mkdir -p symlinked-dist/
	if [ ! -e symlinked/x86 ] ; then ln -s $(TOP)/external/cerbero/build/dist/android_x86/ symlinked-dist/x86 ; fi
	if [ ! -e symlinked/x86_64 ] ; then ln -s $(TOP)/external/cerbero/build/dist/android_x86_64/ symlinked-dist/x86_64 ; fi
	if [ ! -e symlinked/armeabi-v7a ] ; then ln -s $(TOP)/external/cerbero/build/dist/android_armv7/ symlinked-dist/armeabi-v7a ; fi
	if [ ! -e symlinked/arm64-v8a ] ; then ln -s $(TOP)/external/cerbero/build/dist/android_arm64/ symlinked-dist/arm64-v8a ; fi


.PHONY:
copy-lv2-deps:
	make A_ARCH=x86 A_ARCH2=x86 copy-lv2-deps-single
	make A_ARCH=x86_64 A_ARCH2=x86_64 copy-lv2-deps-single
	make A_ARCH=armv7 A_ARCH2=armeabi-v7a copy-lv2-deps-single
	make A_ARCH=arm64 A_ARCH2=arm64-v8a copy-lv2-deps-single

copy-lv2-deps-single:
	for appname in $(APPNAMES) ; do \
		mkdir -p poc-samples/$$appname/app/src/main/jniLibs/$(A_ARCH2) && \
		cp external/cerbero/build/dist/android_$(A_ARCH)/lib/*.so poc-samples/$$appname/app/src/main/jniLibs/$(A_ARCH2)/ && \
		mkdir -p poc-samples/$$appname/app/src/main/assets/lv2/$(A_ARCH) ; \
		cp -R external/cerbero/build/dist/android_$(A_ARCH)/lib/lv2/* poc-samples/$$appname/app/src/main/assets/lv2/ ; \
		rm poc-samples/$$appname/app/src/main/assets/lv2/*/*.so ; \
		cp -R external/cerbero/build/dist/android_$(A_ARCH)/lib/lv2/*/*.so poc-samples/$$appname/app/src/main/jniLibs/$(A_ARCH2) ; \
	done

.PHONY:
build-lv2-deps:
	make CERBERO_COMMAND="build mda-lv2 lilv" run-cerbero-command


.PHONY:
run-cerbero-command:
	cd external/cerbero && for abi in $(ABIS_SIMPLE) ; do \
		ANDROID_NDK_HOME=$(ANDROID_NDK) ./cerbero-uninstalled -c config/cross-android-$$abi.cbc $(CERBERO_COMMAND) ; \
	done

