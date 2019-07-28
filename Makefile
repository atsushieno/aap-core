

ABIS_SIMPLE= x86 x86_64 armeabi-v7a arm64-v8a
ANDROID_NDK=~/Android/Sdk/ndk/20.0.5594570

build-all: \
	build-dependencies \
	build-desktop \
	build-android \
	import-lv2-deps \
	build-java

build-dependencies:
	cd dependencies/cerbero-artifacts && make

build-desktop:
	echo TODO: covers core and lv2, build for testing on desktop
	cd native && mkdir -p build && cd build && cmake .. && make

build-android:
	for abi in $(ABIS_SIMPLE) ; do \
		make A_ABI=$$abi build-android-single ; \
	done

build-android-single:
	mkdir -p native/build-android/$(A_ABI) && cd native/build-android/$(A_ABI) && cmake -DANDROID_STL=c++_shared -DBUILD_SHARED_LIBS=on -DCMAKE_TOOLCHAIN_FILE=$(ANDROID_NDK)/build/cmake/android.toolchain.cmake -DANDROID_ABI=$(A_ABI) -DANDROID_PLATFORM=android-29 ../.. && make

all: build-all

import-lv2-deps:
	bash import-lv2-deps.sh

build-java:
	cd java && ./gradlew assembleDebug

