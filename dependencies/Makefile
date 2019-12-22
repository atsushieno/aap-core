# This Makefile is to build and copy native LV2 dependencies

PWD=$(shell pwd)

ANDROID_NDK=/home/$(USER)/Android/Sdk/ndk/20.0.5594570/
LLVM_TOOLCHAIN=$(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64
ANDROID_TOOLCHAIN=$(ANDROID_NDK)/toolchains/$(ABI_COMPLEX)-4.9/prebuilt/linux-x86_64

TOP=`pwd`

all: build

build: build-native

.PHONY:
clean:
	make -C cerbero-artifacts clean

.PHONY:
prepare:
	cd -C cerbero-artifacts

.PHONY:
build-native: build-lv2-stuff


.PHONY:
build-lv2-stuff:
	make -C cerbero-artifacts
	make ABI_FORMAL=armeabi-v7a ABI_SIMPLE=armv7  ABI_CLANG=armv7a-linux-androideabi ABI_COMPLEX=arm-linux-androideabi build-single-abi
	make ABI_FORMAL=arm64-v8a ABI_SIMPLE=arm64  ABI_CLANG=aarch64-linux-android    ABI_COMPLEX=aarch64-linux-android build-single-abi
	make ABI_FORMAL=x86 ABI_SIMPLE=x86    ABI_CLANG=i686-linux-android       ABI_COMPLEX=i686-linux-android build-single-abi
	make ABI_FORMAL=x86_64 ABI_SIMPLE=x86-64 ABI_CLANG=x86_64-linux-android     ABI_COMPLEX=x86_64-linux-android build-single-abi

.PHONY:
build-single-abi:
	make MODULE=serd MODULE_DIRECTORY=serd MODULE_MAJOR=0 MODULE_VER=0.30.3 MODULE_OPTIONS="--no-utils" build-single
	make MODULE=sord MODULE_DIRECTORY=sord MODULE_MAJOR=0 MODULE_VER=0.16.4 MODULE_OPTIONS="--no-utils" build-single
	make MODULE=lv2 MODULE_DIRECTORY=lv2 MODULE_MAJOR=0 build-single-no-soname-opt
	make MODULE=sratom MODULE_DIRECTORY=sratom MODULE_MAJOR=0 MODULE_VER=0.6.4 build-single
	make MODULE=lilv MODULE_DIRECTORY=lilv MODULE_MAJOR=0 MODULE_VER=0.24.5 MODULE_OPTIONS="--no-utils" build-single
	make MODULE=mda-lv2 MODULE_DIRECTORY=mda-lv2 MODULE_MAJOR=0 build-single-no-soname-opt

.PHONY:
build-single:
	make LDFLAGS="-Wl,-soname,lib$(MODULE)-$(MODULE_MAJOR).so" build-single-no-soname-opt
	mv dist/$(ABI_FORMAL)/lib/lib$(MODULE)-$(MODULE_MAJOR).so.$(MODULE_VER) dist/$(ABI_FORMAL)/lib/lib$(MODULE)-$(MODULE_MAJOR).so
	rm dist/$(ABI_FORMAL)/lib/lib$(MODULE)-$(MODULE_MAJOR).so.0

.PHONY:
build-single-no-soname-opt:
	echo "Building $(MODULE) for $(ABI_FORMAL) ($(ABI_COMPLEX)) ..."
	cd $(MODULE_DIRECTORY) && \
	PKG_CONFIG_PATH=$(PWD)/dist/$(ABI_FORMAL)/lib/pkgconfig:$(PWD)/cerbero-artifacts/outputs/$(ABI_FORMAL)/lib/pkgconfig \
	CC=$(LLVM_TOOLCHAIN)/bin/$(ABI_CLANG)29-clang \
	CXX=$(LLVM_TOOLCHAIN)/bin/$(ABI_CLANG)29-clang++ \
	LD=$(ANDROID_TOOLCHAIN)/bin/$(ABI_COMPLEX)-ld \
	CFLAGS="--sysroot=$(ANDROID_NDK)/sysroot -I$(ANDROID_NDK)/sysroot/usr/include/ -DANDROID" \
	LDFLAGS="-landroid $(LDFLAGS)" \
	./waf -d $(MODULE_OPTIONS) --prefix=../dist/$(ABI_FORMAL) configure build install && \
	cd ..
