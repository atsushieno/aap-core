

ABIS_SIMPLE= x86 x86_64 armeabi-v7a arm64-v8a
ANDROID_NDK=~/Android/Sdk/ndk/20.0.5594570

build-all: \
	build-cerbero-artifacts \
	build-desktop \
	build-android \
	import-lv2-deps \
	build-java

build-cerbero-artifacts:
	cd dependencies/cerbero-artifacts && make

build-desktop:
	echo TODO: covers core and lv2, build for testing on desktop
	cd dependencies/lv2-desktop && bash setup.sh && cd ../..
	mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make

build-android:
	for abi in $(ABIS_SIMPLE) ; do \
		make A_ABI=$$abi build-android-single ; \
	done

build-android-single:
	mkdir -p build-android/$(A_ABI) && cd build-android/$(A_ABI) && cmake -DCMAKE_BUILD_TYPE=Debug -DANDROID_STL=c++_shared -DBUILD_SHARED_LIBS=on -DCMAKE_TOOLCHAIN_FILE=$(ANDROID_NDK)/build/cmake/android.toolchain.cmake -DANDROID_ABI=$(A_ABI) -DCMAKE_ANDROID_ARCH_ABI=$(A_ABI) -DANDROID_PLATFORM=android-29 ../.. && make

all: build-all

import-lv2-deps: build-lv2-importer
	mkdir -p java/samples/aaphostsample/src/main/res/xml
	bash import-lv2-deps.sh

build-lv2-importer:
	cd tools/aap-import-lv2-metadata && rm -rf build && mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make

build-java:
	cd java && ./gradlew assembleDebug

