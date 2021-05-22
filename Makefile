
all: \
	build-desktop \
	build-java

all-no-desktop: \
	build-java

build-desktop:
	mkdir -p build && cd build && cmake -DCMAKE_INSTALL_PREFIX=dist -DCMAKE_BUILD_TYPE=Debug .. && make && make install
	cd docs/native && doxygen && cd ../..

build-java: setup-dummy-prefab-headers-dir
	cd java && ANDROID_SDK_ROOT=$(ANDROID_SDK_ROOT) ./gradlew build dokkaHtml publishToMavenLocal

# FIXME: remove this target once https://issuetracker.google.com/issues/172105145 is supported.
# (NOTE: we can even skip this right now, as prefab is not enabled due to other couple of Google issues)
setup-dummy-prefab-headers-dir:
	mkdir -p dummy-prefab-headers
	cp -R native/plugin-api/include dummy-prefab-headers
	cp -R native/androidaudioplugin/core/include dummy-prefab-headers
	cp -R native/androidaudioplugin/android/include dummy-prefab-headers

