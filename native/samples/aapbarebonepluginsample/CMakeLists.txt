# Sets the minimum version of CMake required to build your native library.
# This ensures that a certain set of CMake features is available to
# your build.

cmake_minimum_required(VERSION 3.6.0)

if (ANDROID)
  set (aap_platform_dep_libraries android log)
endif (ANDROID)

# Specifies a library name, specifies whether the library is STATIC or
# SHARED, and provides relative paths to the source code. You can
# define multiple libraries by adding multiple add_library() commands,
# and CMake builds them for you. When you build your app, Gradle
# automatically packages shared libraries with your APK.

add_library( # Specifies the name of the library.
        aapbarebonepluginsample

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        "aapbarebonepluginsample.cpp"
        )

target_include_directories(aapbarebonepluginsample
        PRIVATE
        "../../plugin-api/include"
        "../../androidaudioplugin/core/include"
        "../../androidaudioplugin/android/src/gen/include"
        )

target_compile_options(aapbarebonepluginsample
        PRIVATE
        -std=c++17
        -Wall
        -Wextra-semi
        -Wshadow
        )

target_link_libraries(aapbarebonepluginsample ${aap_platform_dep_libraries} androidaudioplugin)

