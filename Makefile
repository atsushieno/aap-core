
MINIMIZE_INTERMEDIATES=0
ANDROID_NDK=$(ANDROID_SDK_ROOT)/ndk/21.2.6472646
NDK_HOST=`uname | tr '[:upper:]' '[:lower:]'`


all: \
	maybe-download-ndk \
	build-desktop \
	setup-dummy-prefab-headers-dir \
	build-java

all-no-desktop: \
	maybe-download-ndk \
	setup-dummy-prefab-headers-dir \
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
	cd docs/native && doxygen && cd ../..

build-java:
	cd java && ANDROID_SDK_ROOT=$(ANDROID_SDK_ROOT) ./gradlew build dokka publishToMavenLocal

# FIXME: remove this target once https://issuetracker.google.com/issues/172105145 is supported.
setup-dummy-prefab-headers-dir:
	mkdir -p dummy-prefab-headers
	cp -R native/plugin-api/include dummy-prefab-headers
	cp -R native/androidaudioplugin/core/include dummy-prefab-headers
	cp -R native/androidaudioplugin/android/include dummy-prefab-headers

