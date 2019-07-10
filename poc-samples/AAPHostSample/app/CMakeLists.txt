# Sets the minimum version of CMake required to build your native library.
# This ensures that a certain set of CMake features is available to
# your build.

cmake_minimum_required(VERSION 3.4.1)

# it is not usable until cmake 3.14...
#target_link_directories(aapnativebridge
#        PRIVATE
#        "${Project_SOURCE_DIR}/../../../symlinked-dist/${CMAKE_ANDROID_ARCH_ABI}/lib/"
#        )
link_directories(
        "${Project_SOURCE_DIR}/../../../symlinked-dist/${CMAKE_ANDROID_ARCH_ABI}/lib/"
)

# Specifies a library name, specifies whether the library is STATIC or
# SHARED, and provides relative paths to the source code. You can
# define multiple libraries by adding multiple add_library() commands,
# and CMake builds them for you. When you build your app, Gradle
# automatically packages shared libraries with your APK.

add_library( # Specifies the name of the library.
            androidaudioplugin

            # Sets the library as a shared library.
            SHARED

            # Provides a relative path to your source file(s).
            "${Project_SOURCE_DIR}/../../../src/android-audio-plugin-host.cpp"
            "${Project_SOURCE_DIR}/../../../src/android-audio-plugin-lv2-bridge.cpp"
            "${Project_SOURCE_DIR}/../../../src/android-audio-plugin-client-bridge.cpp"
            "${Project_SOURCE_DIR}/../../../src/android-audio-plugin-local-bridge.cpp"
            "${Project_SOURCE_DIR}/../../../src/gen/org/androidaudiopluginframework/AudioPluginService.cpp"
            "src/main/jni/AAPRemoteBridge.cpp"
            "src/main/jni/LV2DirectUsageSample.cpp"
            "src/main/jni/AAPLV2LocalBridge.cpp")

target_include_directories(androidaudioplugin
        PRIVATE
        "${Project_SOURCE_DIR}/../../../src/gen"
        "${Project_SOURCE_DIR}/../../../src/gen/aidl"
        )

target_compile_options(androidaudioplugin
        PRIVATE
        -std=c++17
        -Wall
        -Wextra-semi
        -Wshadow
        -Wshadow-field
        )

target_include_directories(androidaudioplugin
        PRIVATE
        "${Project_SOURCE_DIR}/../../../symlinked-dist/${CMAKE_ANDROID_ARCH_ABI}/include/"
        "${Project_SOURCE_DIR}/../../../symlinked-dist/${CMAKE_ANDROID_ARCH_ABI}/include/lilv-0"
        )

target_link_libraries(androidaudioplugin android binder_ndk log lilv-0)

