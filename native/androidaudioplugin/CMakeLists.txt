# Sets the minimum version of CMake required to build your native library.
# This ensures that a certain set of CMake features is available to
# your build.

cmake_minimum_required(VERSION 3.5.1)

# List of sources. Android build has some additional sources.
set (androidaudioplugin_SOURCES
	"core/src/android-audio-plugin-host.cpp"
	"../../dependencies/tinyxml2/tinyxml2.cpp"
	)

set (androidaudioplugin_INCLUDES
	"../plugin-api/include"
	"core/include"
	"android/include"
	"android/src/gen/include"
	"../../dependencies/tinyxml2"
	)

#set (androidaudioplugin_LIBS ${androidaudioplugin_LIBS} tinyxml2)

if (ANDROID)

set (androidaudioplugin_SOURCES
	${androidaudioplugin_SOURCES}
	"android/src/android-audio-plugin-client-bridge.cpp"
	"android/src/gen/org/androidaudioplugin/AudioPluginInterface.cpp"
	"android/src/android-context.cpp"
	"android/src/AudioPluginHost_native.cpp"
	"android/src/audio-plugin-host-android.cpp"
	)

set (androidaudioplugin_LIBS ${androidaudioplugin_LIBS} android log binder_ndk)

else (ANDROID)

set (androidaudioplugin_SOURCES
	${androidaudioplugin_SOURCES}
	"desktop/src/desktop-audio-plugin-client-bridge.cpp"
	"desktop/src/AudioPluginService.pb.cc"
	"desktop/src/AudioPluginService.grpc.pb.cc"
	"desktop/src/audio-plugin-host-desktop.cpp"
	)

set (androidaudioplugin_LIBS ${androidaudioplugin_LIBS} dl grpc grpc++ protobuf rt)
endif (ANDROID)

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
        -Wshadow
        )

target_link_libraries (androidaudioplugin ${androidaudioplugin_LIBS})
