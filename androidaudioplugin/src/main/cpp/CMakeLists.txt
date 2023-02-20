cmake_minimum_required(VERSION 3.5.1)

project(androidaudioplugin VERSION 0.6.2 LANGUAGES CXX)

# List of sources. Android build has some additional sources.
set (androidaudioplugin_SOURCES
	"core/hosting/audio-plugin-host.cpp"
	"core/hosting/plugin-client-system.cpp"
	"core/hosting/plugin-connections.cpp"
	"core/aapxs/extension-service.cpp"
	"core/extensions/extension-service-impl.cpp"
	"core/extensions/presets-service.cpp"
	"core/extensions/state-service.cpp"
	"core/extensions/port-config-service.cpp"
	"core/extensions/gui-service.cpp"
	)

if (ANDROID)
set (androidaudioplugin_SOURCES
	${androidaudioplugin_SOURCES}
	"android/binder-client-as-plugin.cpp"
	"android/gen/org/androidaudioplugin/AudioPluginInterface.cpp"
	"android/gen/org/androidaudioplugin/AudioPluginInterfaceCallback.cpp"
	"android/android-application-context.cpp"
	"android/AudioPluginNatives_jni.cpp"
	"android/audio-plugin-host-android-internal.cpp"
	"android/AAPJniFacade.cpp"
	"android/ALooperMessage.cpp"
	)
else (ANDROID)
set (androidaudioplugin_SOURCES
	${androidaudioplugin_SOURCES}
	desktop/audio-plugin-host-desktop-internal.cpp
)
endif (ANDROID)

add_library (androidaudioplugin
  SHARED
  ${androidaudioplugin_SOURCES}
  )

target_compile_options (androidaudioplugin
		PRIVATE
		-std=c++17 -Wall -Wshadow
		# see https://issuetracker.google.com/issues/219987524#comment37
		-Werror=unguarded-availability
		)

target_compile_definitions (androidaudioplugin
		PUBLIC
		__ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__=1
		)

target_include_directories (androidaudioplugin
		PRIVATE
		"../../../../include/"
		)

if (ANDROID)
target_include_directories (androidaudioplugin
		PRIVATE
		"android/gen/include"
		)

target_link_libraries (androidaudioplugin
		android
		log
		binder_ndk
		)
endif (ANDROID)

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
