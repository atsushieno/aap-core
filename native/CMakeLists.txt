# Sets the minimum version of CMake required to build your native library.
# This ensures that a certain set of CMake features is available to
# your build.

cmake_minimum_required(VERSION 3.6.0)

if (NOT ANDROID)
	add_subdirectory ("samples")
endif (NOT ANDROID)

# List of sources. Android build has some additional sources.
set (androidaudioplugin_SOURCES
	"core/src/android-audio-plugin-host.cpp"
	"core/src/android-audio-plugin-local-bridge.cpp"
	"lv2/src/android-audio-plugin-lv2-bridge.cpp"
	"../dependencies/tinyxml2/tinyxml2.cpp"
	)

set (androidaudioplugin_INCLUDES
	"plugin-api/include"
	"core/include"
	"android/src/gen/include"
	"../dependencies/tinyxml2"
	"../dependencies/cerbero-artifacts/outputs/${CMAKE_ANDROID_ARCH_ABI}/include/"
	"../dependencies/cerbero-artifacts/outputs/${CMAKE_ANDROID_ARCH_ABI}/include/lilv-0"
	)

#set (androidaudioplugin_LIBS ${androidaudioplugin_LIBS} tinyxml2)

if (ANDROID)

set (ENV{PKG_CONFIG_PATH} "../dependencies/cerbero-artifacts/outputs/${CMAKE_ANDROID_ARCH_ABI}/lib/pkgconfig/")

set (androidaudioplugin_SOURCES
	${androidaudioplugin_SOURCES}
	"android/src/android-audio-plugin-client-bridge.cpp"
	"android/src/gen/org/androidaudioplugin/AudioPluginInterface.cpp"
	"android/src/AudioPluginHost_native.cpp"
	)

set (androidaudioplugin_LIBS ${androidaudioplugin_LIBS} android log binder_ndk)

else (ANDROID)

# it is not usable until cmake 3.14...
#target_link_directories (androidaudioplugin ...)
link_directories (
		"../dependencies/lv2-desktop/dist/lib/"
)

endif (ANDROID)

if (NOT ISOLATE_LV2)

	if (ANDROID)

		# it is not usable until cmake 3.14...
		#target_link_directories (androidaudioplugin ...)
		link_directories (
				"../dependencies/cerbero-artifacts/outputs/${CMAKE_ANDROID_ARCH_ABI}/lib/"
		)

	else (ANDROID)

		set (ENV{PKG_CONFIG_PATH} "../dependencies/lv2-desktop/dist/lib/pkgconfig/")

	endif (ANDROID)

find_package ( PkgConfig REQUIRED )

# Mandatory libraries: serd, sord, lilv
pkg_check_modules ( SERD REQUIRED serd-0>=0.28.0 )
pkg_check_modules ( SORD REQUIRED sord-0>=0.16.0 )
pkg_check_modules ( SRATOM REQUIRED sratom-0>=0.6.0 )
pkg_check_modules ( LILV REQUIRED lilv-0>=0.24.0 )

set (androidaudioplugin_SOURCES
	${androidaudioplugin_SOURCES}
	"lv2/src/android-audio-plugin-lv2-bridge.cpp"
	"lv2/src/AudioPluginLV2LocalHost_native.cpp"
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


