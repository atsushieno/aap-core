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
             aaphost

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             src/main/jni/aaphostnative.cpp)

target_compile_options(aaphost
        PRIVATE
        -std=c++17
        -Wall
        -Wextra-semi
        -Wshadow
        -Wshadow-field
        )

target_include_directories(aaphost
        PRIVATE
        "${Project_SOURCE_DIR}/../../../symlinked-dist/${CMAKE_ANDROID_ARCH_ABI}/include/"
        "${Project_SOURCE_DIR}/../../../symlinked-dist/${CMAKE_ANDROID_ARCH_ABI}/include/lilv-0"
        )

target_link_libraries(aaphost binder_ndk log)

target_link_libraries(aaphost lilv-0)

