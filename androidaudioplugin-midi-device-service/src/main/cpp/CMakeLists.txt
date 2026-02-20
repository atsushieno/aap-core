cmake_minimum_required(VERSION 3.5.1)

project(aapmidideviceservice LANGUAGES CXX)

set (TOPDIR "../../../..")

include(cmake/CPM.cmake)

set(LIBREMIDI_NO_ANDROID ON)

# Ensure midicci is built as a static library regardless of parent BUILD_SHARED_LIBS setting
set(_AAPMIDS_OLD_BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS}")
set(BUILD_SHARED_LIBS OFF)
CPMAddPackage(
        NAME            midicci
        GIT_TAG         4e2ecfb
        GIT_REPOSITORY  https://github.com/atsushieno/midicci
        OPTIONS         "MIDICCI_SKIP_TOOLS ON"
        EXCLUDE_FROM_ALL
)
# Restore BUILD_SHARED_LIBS
if(DEFINED _AAPMIDS_OLD_BUILD_SHARED_LIBS AND NOT "${_AAPMIDS_OLD_BUILD_SHARED_LIBS}" STREQUAL "")
    set(BUILD_SHARED_LIBS "${_AAPMIDS_OLD_BUILD_SHARED_LIBS}")
else()
    unset(BUILD_SHARED_LIBS)
endif()

# List of sources. Android build has some additional sources.
set (aapmidideviceservice_SOURCES
	"aapmidideviceservice-jni.cpp"
	"AAPMidiProcessor.cpp"
	"AAPMidiProcessor_android.cpp"
	"AAPMidiCISession.cpp"
	"zix/ring.cpp"
	)

add_library ( # Specifies the name of the library.
		aapmidideviceservice

		# Sets the library as a shared library.
		SHARED

		# Provides a relative path to your source file(s).
		${aapmidideviceservice_SOURCES}
		)

find_package(oboe REQUIRED CONFIG)
find_package(androidaudioplugin REQUIRED CONFIG)

set (aapmidideviceservice_INCLUDES
		.
		${TOPDIR}/external/cmidi2
		${TOPDIR}/include
		${midicci_SOURCE_DIR}/include
		)

target_include_directories (aapmidideviceservice
        PRIVATE
        ${aapmidideviceservice_INCLUDES}
        )

target_compile_options (aapmidideviceservice
        PRIVATE
        -std=c++20
        -Wall
        -Wshadow
        # uncomment this if you want to enable AddressSanitizer
        #-fsanitize=address -fno-omit-frame-pointer
		-DHAVE_MLOCK=1
        )

target_link_libraries (aapmidideviceservice
		PRIVATE
		android
		log
		binder_ndk
		oboe::oboe
		androidaudioplugin::androidaudioplugin
		midicci_static
		)

target_compile_definitions (aapmidideviceservice
		PUBLIC
		ANDROID=${ANDROID}
		)


# You can set it via build.gradle
if (${AAP_ENABLE_ASAN})
	target_compile_options (aapmidideviceservice
			PUBLIC
			-fsanitize=address -fno-omit-frame-pointer
			)

	target_link_options(aapmidideviceservice
			PUBLIC
			-fsanitize=address
			)

	set_target_properties(aapmidideviceservice
			PROPERTIES LINK_FLAGS -fsanitize=address
			)
endif()
