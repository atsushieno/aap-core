cmake_minimum_required(VERSION 3.5.1)

project(androidaudioplugin VERSION 0.6.2 LANGUAGES CXX)

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
	"desktop/src/gen/AudioPluginService.pb.cc"
	"desktop/src/gen/AudioPluginService.grpc.pb.cc"
	"desktop/src/audio-plugin-host-desktop.cpp"
	"desktop/src/AudioPluginInterfaceImpl.cpp"
	)

set (androidaudioplugin_LIBS ${androidaudioplugin_LIBS} dl grpc grpc++ protobuf rt)

endif (ANDROID)


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
