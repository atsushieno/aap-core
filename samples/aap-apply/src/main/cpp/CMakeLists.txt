# Sets the minimum version of CMake required to build your native library.
# This ensures that a certain set of CMake features is available to
# your build.

cmake_minimum_required(VERSION 3.6.0)

# Specifies a library name, specifies whether the library is STATIC or
# SHARED, and provides relative paths to the source code. You can
# define multiple libraries by adding multiple add_library() commands,
# and CMake builds them for you. When you build your app, Gradle
# automatically packages shared libraries with your APK.

set(LABSOUND_DIR "../../../../../external/LabSound")

add_subdirectory("${LABSOUND_DIR}" "labsound")

add_library( # Specifies the name of the library.
        aapapply

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        "aap-apply.cpp"
        )

target_include_directories(aapapply
        PRIVATE
        ../../../../../include
        include
        ${LABSOUND_DIR}/include
        )

target_compile_options(aapapply
        PRIVATE
        -Wall
        -Wextra-semi
        -Wshadow
        -Wshadow-field
        # uncomment this if you want to enable AddressSanitizer
        #-fsanitize=address -fno-omit-frame-pointer
        )

if (BUILD_WITH_PREFAB)
if (ANDROID)
    find_package (androidaudioplugin REQUIRED CONFIG)
    set (aapapply_LIBS
            android
            log
            androidaudioplugin::androidaudioplugin)
else (ANDROID)
    set (aapapply_LIBS)
endif (ANDROID)
target_link_libraries(aapapply ${aapapply_LIBS})
else ()
target_link_libraries(aapapply android log LabSound)
endif ()

# uncomment this if you want to enable AddressSanitizer
#set_target_properties(aap-apply PROPERTIES LINK_FLAGS -fsanitize=address)

