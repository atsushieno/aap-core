
MINIMIZE_INTERMEDIATES=0
ABIS_SIMPLE= x86 x86_64 armeabi-v7a arm64-v8a
ANDROID_NDK=$(ANDROID_SDK_ROOT)/ndk/21.2.6472646
NDK_HOST=`uname | tr '[:upper:]' '[:lower:]'`


all: \
	maybe-download-ndk \
	build-desktop \
	build-java

.PHONY:
maybe-download-ndk: $(ANDROID_NDK)

$(ANDROID_NDK):
	if ! [ -d "$(ANDROID_SDK_ROOT)" ] ; then \
		echo "ANDROID_SDK_ROOT is not defined." ; \
		exit 1 ; \
	fi
	echo "Android NDK will be installed as $(ANDROID_NDK)"
	wget https://dl.google.com/android/repository/android-ndk-r21c-$(NDK_HOST)-x86_64.zip >/dev/null
	unzip android-ndk-r21c-$(NDK_HOST)-x86_64.zip >/dev/null
	mkdir -p $(ANDROID_NDK)
	mv android-ndk-r21c/* $(ANDROID_NDK)
	if [ $MINIMIZE_INTERMEDIATES ] ; then \
		rm android-ndk-r21c-$(NDK_HOST)-x86_64.zip ; \
	fi

build-desktop:
	mkdir -p build && cd build && cmake -DCMAKE_INSTALL_PREFIX=dist -DCMAKE_BUILD_TYPE=Debug .. && make && make install
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

build-java:
	cd java && ANDROID_SDK_ROOT=$(ANDROID_SDK_ROOT) ./gradlew build dokka

