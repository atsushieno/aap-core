cmake_minimum_required(VERSION 3.5.1)

project(aapmidideviceservice VERSION 0.6.2 LANGUAGES CXX)

set (TOPDIR "../../../../..")

set (OBOE_DIR "${TOPDIR}/external/oboe")

add_subdirectory(${OBOE_DIR} ./oboe)

include_directories (${OBOE_DIR}/include)

# List of sources. Android build has some additional sources.
set (aapmidideviceservice_SOURCES
	"aapmidideviceservice-jni.cpp"
	"AAPMidiProcessor.cpp"
	"AAPMidiProcessor_android.cpp"
	"zix/ring.cpp"
	)

set (aapmidideviceservice_INCLUDES
		.
		${TOPDIR}/external/cmidi2
		${TOPDIR}/native/plugin-api/include
		${TOPDIR}/native/androidaudioplugin/core/include
		${TOPDIR}/native/androidaudioplugin/android/include
		)

set (aapmidideviceservice_LIBDIRS
		# FIXME: do not depend on `debug` here.
		"${TOPDIR}/java/androidaudioplugin/build/intermediates/merged_native_libs/debug/out/lib/${CMAKE_ANDROID_ARCH_ABI}"
		)

set (aapmidideviceservice_LIBS
		androidaudioplugin
		android
		log
		binder_ndk
		oboe
		)

add_library ( # Specifies the name of the library.
		aapmidideviceservice

		# Sets the library as a shared library.
		SHARED

		# Provides a relative path to your source file(s).
		${aapmidideviceservice_SOURCES}
		)

target_include_directories (aapmidideviceservice
        PRIVATE
        ${aapmidideviceservice_INCLUDES}
        )

target_compile_options (aapmidideviceservice
        PRIVATE
        -std=c++17
        -Wall
        -Wshadow
        # uncomment this if you want to enable AddressSanitizer
        #-fsanitize=address -fno-omit-frame-pointer
		-DHAVE_MLOCK=1
        )

target_link_directories(aapmidideviceservice
		PRIVATE
		${aapmidideviceservice_LIBDIRS}
		)

target_link_libraries (aapmidideviceservice
		${aapmidideviceservice_LIBS}
		)

# uncomment this if you want to enable AddressSanitizer
#set_target_properties(aapmidideviceservice PROPERTIES LINK_FLAGS -fsanitize=address)
