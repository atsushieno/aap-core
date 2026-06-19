cmake_minimum_required(VERSION 3.5.1)

project(androidaudioplugin-js-controller LANGUAGES CXX)

set (androidaudioplugin-js-controller_SOURCES
        AapJsControllerRuntime.cpp
        JNI.cpp
    )

find_package(androidaudioplugin REQUIRED CONFIG)

add_library (androidaudioplugin-js-controller
        SHARED
        ${androidaudioplugin-js-controller_SOURCES}
    )

# choc's QuickJS context is an amalgamated single-header build; it needs C++17 and is large,
# so we keep warnings non-fatal here (the embedded QuickJS sources are third-party).
target_compile_options (androidaudioplugin-js-controller
        PRIVATE
        -std=c++17
        )

target_compile_definitions (androidaudioplugin-js-controller
        PUBLIC
        __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__=1
        )

target_include_directories (androidaudioplugin-js-controller
        PRIVATE
        "../../../../include/"
        "../../../../external/choc"
        )

if (ANDROID)
target_link_libraries (androidaudioplugin-js-controller
        androidaudioplugin::androidaudioplugin
        log
        android
        )
endif (ANDROID)
