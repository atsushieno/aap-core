cmake_minimum_required(VERSION 3.4.1)

enable_language ( CXX )

set (aap-android_sources
	src/android-audio-plugin-client-bridge.cpp
	src/gen/org/androidaudiopluginframework/AudioPluginInterface.cpp)

add_library(aap-android ${aap-android_sources})

target_include_directories(aap-android
	PRIVATE src src/gen/include ../common/include ../aap/include
	PUBLIC include)

target_compile_options(aap-android
	PRIVATE
	-std=c++17
	-Wall
	-Wextra-semi
	-Wshadow
	-Wshadow-field
	-Ofast
	"$<$<CONFIG:DEBUG>:-Werror>")

# target_compile_definitions(aap-android PUBLIC "$<$<CONFIG:DEBUG>:ANY_ADDITIONAL_DEFS>")

target_link_libraries(aap-android PRIVATE -landroid -lc++ -lbinder_ndk)
