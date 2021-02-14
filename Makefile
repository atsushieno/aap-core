
all: \
	build-desktop \
	setup-dummy-prefab-headers-dir \
	build-java

all-no-desktop: \
	setup-dummy-prefab-headers-dir \
	build-java

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

