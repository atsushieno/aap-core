# Sets the minimum version of CMake required to build your native library.
# This ensures that a certain set of CMake features is available to
# your build.

cmake_minimum_required(VERSION 3.6.0)

# it is not usable until cmake 3.14...
#target_link_directories(aapnativebridge
link_directories(
        "../../androidaudioplugin/build/intermediates/merged_native_libs/debug/out/lib/${CMAKE_ANDROID_ARCH_ABI}"
)

# Specifies a library name, specifies whether the library is STATIC or
# SHARED, and provides relative paths to the source code. You can
# define multiple libraries by adding multiple add_library() commands,
# and CMake builds them for you. When you build your app, Gradle
# automatically packages shared libraries with your APK.

add_library( # Specifies the name of the library.
        aaphostsample

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        # FIXME: this is not a valid sample anymore. It accesses AIDL which is NOT public.
        # replace with aap::PluginHost and co.
        "src/main/jni/AAPSampleRemoteInterop_native.cpp"
        )

target_include_directories(aaphostsample
        PRIVATE
        "${Project_SOURCE_DIR}/../../../native/plugin-api/include"
        "${Project_SOURCE_DIR}/../../../native/androidaudioplugin/core/include"
        # FIXME: it should have no access to AIDL generated code.
        "${Project_SOURCE_DIR}/../../../native/androidaudioplugin/android/src/gen/include"
        "${Project_SOURCE_DIR}/../../../dependencies/dist/${CMAKE_ANDROID_ARCH_ABI}/include/"
        "${Project_SOURCE_DIR}/../../../dependencies/dist/${CMAKE_ANDROID_ARCH_ABI}/include/lilv-0"
        )

target_compile_options(aaphostsample
        PRIVATE
        -std=c++17
        -Wall
        -Wextra-semi
        -Wshadow
        -Wshadow-field
        )

if (FALSE)
if (ANDROID)
    find_package (androidaudioplugin REQUIRED CONFIG)
    set (aaphostsample_LIBS
            android
            binder_ndk
            log
            androidaudioplugin::androidaudioplugin)
else (ANDROID)
    set (aaphostsample_LIBS
            androidaudioplugin)
endif (ANDROID)
target_link_libraries(aaphostsample ${aaphostsample_LIBS})
else ()
target_link_libraries(aaphostsample android binder_ndk log androidaudioplugin)
endif ()
