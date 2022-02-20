cmake_minimum_required(VERSION 3.5.1)

project(androidaudioplugin VERSION 0.6.2 LANGUAGES CXX)

# List of sources. Android build has some additional sources.
set (androidaudioplugin_SOURCES
	"core/audio-plugin-host.cpp"
	)

set (androidaudioplugin_INCLUDES
	"../../../../include/"
	"android/gen/include"
	)

set (androidaudioplugin_SOURCES
	${androidaudioplugin_SOURCES}
	"android/binder-client-as-plugin.cpp"
	"android/gen/org/androidaudioplugin/AudioPluginInterface.cpp"
	"android/android-application-context.cpp"
	"android/AudioPluginNatives_jni.cpp"
	"android/audio-plugin-host-android.cpp"
	)

set (androidaudioplugin_LIBS ${androidaudioplugin_LIBS} android log binder_ndk)

add_library (androidaudioplugin
  SHARED
  ${androidaudioplugin_SOURCES}
  )

target_include_directories (androidaudioplugin
  PRIVATE
  ${androidaudioplugin_INCLUDES})

target_compile_options (androidaudioplugin
  PRIVATE
  -std=c++17 -Wall -Wshadow
  )

target_link_libraries (androidaudioplugin
  ${androidaudioplugin_LIBS}
  )

# You can set it via build.gradle.
if (${AAP_ENABLE_ASAN})
target_compile_options (androidaudioplugin
		PUBLIC
		-fsanitize=address -fno-omit-frame-pointer
		)

target_link_options(androidaudioplugin
		PUBLIC
		-fsanitize=address
		)

set_target_properties(androidaudioplugin
		PROPERTIES LINK_FLAGS -fsanitize=address
		)
endif()
