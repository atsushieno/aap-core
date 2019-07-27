

ABIS_SIMPLE= x86 x86_64 armeabi-v7a arm64-v8a

all: build-all

.PHONY:
build-all:
	for abi in $(ABIS_SIMPLE) ; do \
		make A_ABI=$$abi build-single ; \
	done

build-single:
	mkdir -p build/$(A_ABI) && cd build/$(A_ABI) && cmake -DANDROID_STL=c++_shared -DBUILD_SHARED_LIBS=on -DCMAKE_TOOLCHAIN_FILE=../../../../dependencies/cerbero-artifacts/cerbero/build/android-ndk-20/build/cmake/android.toolchain.cmake -DANDROID_ABI=$(A_ABI) -DANDROID_PLATFORM=android-29 ../.. && make
