

all: build-all

build-all: \
	build-dependencies \
	build-libaap \
	build-libaap-lv2 \
	build-libaap-android \
	build-java

build-dependencies:
	cd dependencies/cerbero-artifacts && make

build-libaap:
	echo TODO: we will come up with Android per-ABI build.

build-libaap-lv2:
	echo TODO: we will come up with Android per-ABI build.

build-libaap-android:
	cd native/aap-android && make

build-java:
	cd java && ./gradlew assembleDebug

