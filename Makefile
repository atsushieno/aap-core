# Targets that are used externally

all: build-java

publish-global: publish-global-java

all-no-desktop: build-java

# internals

build-java: setup-dummy-prefab-headers-dir
	cd java && ANDROID_SDK_ROOT=$(ANDROID_SDK_ROOT) ./gradlew build dokkaHtml publishToMavenLocal

publish-global-java:
	cd java && ANDROID_SDK_ROOT=$(ANDROID_SDK_ROOT) ./gradlew build dokkaHtml publish
	

# FIXME: remove this target once https://issuetracker.google.com/issues/172105145 is supported.
# (NOTE: we can even skip this right now, as prefab is not enabled due to other couple of Google issues)
setup-dummy-prefab-headers-dir:
	mkdir -p dummy-prefab-headers
	cp -R native/plugin-api/include dummy-prefab-headers
	cp -R native/androidaudioplugin/core/include dummy-prefab-headers
	cp -R native/androidaudioplugin/android/include dummy-prefab-headers

