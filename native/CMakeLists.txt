# Sets the minimum version of CMake required to build your native library.
# This ensures that a certain set of CMake features is available to
# your build.

cmake_minimum_required(VERSION 3.4.1)

# it is not usable until cmake 3.14...
#target_link_directories (androidaudioplugin ...)
link_directories (
	"${Project_SOURCE_DIR}/../dependencies/cerbero-artifacts/outputs/${CMAKE_ANDROID_ARCH_ABI}/lib/"
)

# List of sources. Android build has some additional sources.
set (androidaudioplugin_SOURCES
	"${Project_SOURCE_DIR}/../native/core/src/android-audio-plugin-host.cpp"
	"${Project_SOURCE_DIR}/../native/core/src/android-audio-plugin-local-bridge.cpp"
	"${Project_SOURCE_DIR}/../native/lv2/src/android-audio-plugin-lv2-bridge.cpp"
	)

set (androidaudioplugin_INCLUDES
	"${Project_SOURCE_DIR}/../native/plugin-api/include"
	"${Project_SOURCE_DIR}/../native/core/include"
	"${Project_SOURCE_DIR}/../native/android/src/gen/include"
	"${Project_SOURCE_DIR}/../dependencies/cerbero-artifacts/outputs/${CMAKE_ANDROID_ARCH_ABI}/include/"
	"${Project_SOURCE_DIR}/../dependencies/cerbero-artifacts/outputs/${CMAKE_ANDROID_ARCH_ABI}/include/lilv-0"
	)

if (ANDROID)

set (ENV{PKG_CONFIG_PATH} "${Project_SOURCE_DIR}/../dependencies/cerbero-artifacts/outputs/${CMAKE_ANDROID_ARCH_ABI}/lib/pkgconfig/")

set (androidaudioplugin_SOURCES
	${androidaudioplugin_SOURCES}
	"${Project_SOURCE_DIR}/../native/android/src/android-audio-plugin-client-bridge.cpp"
	"${Project_SOURCE_DIR}/../native/android/src/gen/org/androidaudioplugin/AudioPluginInterface.cpp"
	)

set (androidaudioplugin_LIBS ${androidaudioplugin_LIBS} android log binder_ndk)

endif (ANDROID)

if (NOT ISOLATE_LV2)

find_package ( PkgConfig REQUIRED )

# Mandatory libraries: serd, sord, lilv
pkg_check_modules ( SERD REQUIRED serd-0>=0.28.0 )
pkg_check_modules ( SORD REQUIRED sord-0>=0.16.0 )
pkg_check_modules ( SRATOM REQUIRED sratom-0>=0.6.0 )
pkg_check_modules ( LILV REQUIRED lilv-0>=0.24.0 )

set (androidaudioplugin_SOURCES
	${androidaudioplugin_SOURCES}
	lv2/src/android-audio-plugin-lv2-bridge.cpp
	)

set (androidaudioplugin_INCLUDES
        ${androidaudioplugin_INCLUDES}
	${SERD_INCLUDE_DIRS}
	${SORD_INCLUDE_DIRS}
	${SRATOM_INCLUDE_DIRS}
	${LILV_INCLUDE_DIRS}
        )

set (androidaudioplugin_LIBS ${androidaudioplugin_LIBS} lilv-0)

endif (NOT ISOLATE_LV2)

# Specifies a library name, specifies whether the library is STATIC or
# SHARED, and provides relative paths to the source code. You can
# define multiple libraries by adding multiple add_library() commands,
# and CMake builds them for you. When you build your app, Gradle
# automatically packages shared libraries with your APK.

add_library ( # Specifies the name of the library.
            androidaudioplugin

            # Sets the library as a shared library.
            SHARED

            # Provides a relative path to your source file(s).
            ${androidaudioplugin_SOURCES}
            )

target_include_directories (androidaudioplugin
        PRIVATE
        ${androidaudioplugin_INCLUDES}
        )

target_compile_options (androidaudioplugin
        PRIVATE
        -std=c++17
	-Wall
        -Wextra-semi
        -Wshadow
        )

target_link_libraries (androidaudioplugin ${androidaudioplugin_LIBS})


