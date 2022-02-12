cmake_minimum_required(VERSION 3.5.1)

project(androidaudioplugin VERSION 0.6.2 LANGUAGES CXX)

# List of sources. Android build has some additional sources.
set (androidaudioplugin_SOURCES
	"core/src/audio-plugin-host.cpp"
	)

set (androidaudioplugin_INCLUDES
	"../plugin-api/include/"
	"core/include"
	"android/include"
	"android/src/gen/include"
	)

set (androidaudioplugin_SOURCES
	${androidaudioplugin_SOURCES}
	"android/src/binder-client-as-plugin.cpp"
	"android/src/gen/org/androidaudioplugin/AudioPluginInterface.cpp"
	"android/src/android-application-context.cpp"
	"android/src/AudioPluginNatives_jni.cpp"
	"android/src/audio-plugin-host-android.cpp"
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

# set non-zero this if you want to debug with AddressSanitizer
set(AAP_DEBUG_WITH_ASAN 0)

if (AAP_DEBUG_WITH_ASAN)
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
