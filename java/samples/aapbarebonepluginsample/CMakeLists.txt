# Sets the minimum version of CMake required to build your native library.
# This ensures that a certain set of CMake features is available to
# your build.

cmake_minimum_required(VERSION 3.6.0)

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
        "src/main/jni/aapbarebonepluginsample.cpp"
        )

target_include_directories(aapbarebonepluginsample
        PRIVATE
        "${Project_SOURCE_DIR}/../../../native/plugin-api/include"
        "${Project_SOURCE_DIR}/../../../native/androidaudioplugin/core/include"
        "${Project_SOURCE_DIR}/../../../native/androidaudioplugin/android/src/gen/include"
        )

target_compile_options(aapbarebonepluginsample
        PRIVATE
        -std=c++17
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
    set (aapbarebonepluginsample_LIBS
            android
            log
            androidaudioplugin::androidaudioplugin)
else (ANDROID)
    set (aapbarebonepluginsample_LIBS)
endif (ANDROID)
target_link_libraries(aapbarebonepluginsample ${aapbarebonepluginsample_LIBS})
else ()
target_link_libraries(aapbarebonepluginsample android log)
endif ()

# uncomment this if you want to enable AddressSanitizer
#set_target_properties(aapbarebonepluginsample PROPERTIES LINK_FLAGS -fsanitize=address)

