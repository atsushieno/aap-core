# Sets the minimum version of CMake required to build your native library.
# This ensures that a certain set of CMake features is available to
# your build.

cmake_minimum_required(VERSION 3.14.0)

project(aapsamplehostengine2)

set (TOPDIR "../../../../..")

set (OBOE_DIR "${TOPDIR}/external/oboe")

add_subdirectory(${OBOE_DIR} ./oboe)

set (aapsamplehostengine2_SOURCES
        AAPSampleHostEngine-jni.cpp
        AAPSampleHostEngine.cpp
        AAPSampleHostEngine_android.cpp
        zix/ring.cpp
        )

add_library( # Specifies the name of the library.
        aapsamplehostengine2

        # Sets the library as a shared library.
        SHARED

        ${aapsamplehostengine2_SOURCES}
        )

find_package(androidaudioplugin REQUIRED CONFIG)

# it is not usable until cmake 3.14...
#target_link_directories(aapsamplehostengine2
#        "../../androidaudioplugin/build/intermediates/merged_native_libs/debug/out/lib/${CMAKE_ANDROID_ARCH_ABI}"
#        )

target_include_directories(aapsamplehostengine2
        PRIVATE
        .
        "${TOPDIR}/include"
        ${TOPDIR}/external/cmidi2
        ${OBOE_DIR}/include
        )

target_compile_options(aapsamplehostengine2
        PRIVATE
        -std=c++17
        -Wall
        -Wextra-semi
        -Wshadow
        -Wshadow-field
        )

if (ANDROID)
    if (BUILD_WITH_PREFAB)
        find_package (androidaudioplugin REQUIRED CONFIG)
        set (aapsamplehostengine2_LIBS
                android
                binder_ndk
                log
                oboe
                androidaudioplugin::androidaudioplugin)
    else (BUILD_WITH_PREFAB)
        target_link_libraries(aapsamplehostengine2 android binder_ndk log androidaudioplugin)
    endif (BUILD_WITH_PREFAB)
else (ANDROID)
    set (aapsamplehostengine2_LIBS androidaudioplugin)
endif ()
target_link_libraries(aapsamplehostengine2 ${aapsamplehostengine2_LIBS})


# You can set it via build.gradle
if (${AAP_ENABLE_ASAN})
    target_compile_options (aapsamplehostengine2
            PUBLIC
            -fsanitize=address -fno-omit-frame-pointer
            )

    target_link_options(aapsamplehostengine2
            PUBLIC
            -fsanitize=address
            )

    set_target_properties(aapsamplehostengine2
            PROPERTIES LINK_FLAGS -fsanitize=address
            )
endif()
