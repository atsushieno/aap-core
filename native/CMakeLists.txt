# Sets the minimum version of CMake required to build your native library.
# This ensures that a certain set of CMake features is available to
# your build.

cmake_minimum_required(VERSION 3.5.1)

add_subdirectory ("androidaudioplugin")
add_subdirectory ("androidaudioplugin-lv2")

if (NOT ANDROID)
	add_subdirectory ("samples")
endif (NOT ANDROID)

