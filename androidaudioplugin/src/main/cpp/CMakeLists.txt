cmake_minimum_required(VERSION 3.5.1)

project(androidaudioplugin LANGUAGES CXX)

# List of sources. Android build has some additional sources.
# FIXME: remove these "avoid vectorization bug" hacks once this issue is resolved:
#  https://github.com/atsushieno/aap-core/issues/194
set (androidaudioplugin_SOURCES_NO_VECTORIZE
		"core/hosting/aap_midi2_helper.cpp"
)
set (androidaudioplugin_SOURCES_VECTORIZE
	"core/hosting/AAPXSMidi2InitiatorSession.cpp"
	"core/hosting/AAPXSMidi2RecipientSession.cpp"
	"core/hosting/PluginInformation.cpp"
	"core/hosting/PluginInstance.cpp"
	"core/hosting/PluginInstance.Local.cpp"
	"core/hosting/PluginInstance.Remote.cpp"
	"core/hosting/audio-plugin-host.cpp"
	"core/hosting/PluginHost.cpp"
	"core/hosting/PluginHost.Client.cpp"
	"core/hosting/PluginHost.Service.cpp"
	"core/hosting/plugin-client-system.cpp"
	"core/hosting/plugin-connections.cpp"
	"core/aapxs/aapxs-runtime.cpp"
	"core/aapxs/gui-aapxs.cpp"
	"core/aapxs/midi-aapxs.cpp"
	"core/aapxs/parameters-aapxs.cpp"
	"core/aapxs/presets-aapxs.cpp"
	"core/aapxs/state-aapxs.cpp"
	"core/aapxs/standard-extensions.cpp"
	"core/aapxs/urid-aapxs.cpp"
)
set_source_files_properties(${androidaudioplugin_SOURCES_VECTORIZE} PROPERTIES COMPILE_FLAGS -fvectorize)
set (androidaudioplugin_SOURCES
		${androidaudioplugin_SOURCES_NO_VECTORIZE}
		${androidaudioplugin_SOURCES_VECTORIZE}
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
		"../../../../external/cmidi2/"
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
