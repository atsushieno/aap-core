cmake_minimum_required(VERSION 3.14.0)

add_library( # Specifies the name of the library.
        androidaudioplugin-sample-host-engine

        # Sets the library as a shared library.
        SHARED

        # FIXME: this is not a valid sample anymore. It accesses AIDL which is NOT public.
        # replace with aap::PluginHost and co.
        "src/main/cpp/AAPSampleRemoteInterop_native.cpp"
        )

target_link_directories(
        androidaudioplugin-sample-host-engine
        PRIVATE
        "../androidaudioplugin/build/intermediates/merged_native_libs/debug/out/lib/${CMAKE_ANDROID_ARCH_ABI}"
)

target_include_directories(androidaudioplugin-sample-host-engine
        PRIVATE
        "${Project_SOURCE_DIR}/../../native/plugin-api/include"
        "${Project_SOURCE_DIR}/../../native/androidaudioplugin/core/include"
        # FIXME: it should have no access to AIDL generated code.
        "${Project_SOURCE_DIR}/../../native/androidaudioplugin/android/src/gen/include"
        "${Project_SOURCE_DIR}/../../dependencies/dist/${CMAKE_ANDROID_ARCH_ABI}/include/"
        "${Project_SOURCE_DIR}/../../../dependencies/dist/${CMAKE_ANDROID_ARCH_ABI}/include/lilv-0"
        )

target_compile_options(androidaudioplugin-sample-host-engine
        PRIVATE
        -std=c++17
        -Wall
        -Wextra-semi
        -Wshadow
        -Wshadow-field
        )

if (BUILD_WITH_PREFAB)
if (ANDROID)
    find_package (androidaudioplugin REQUIRED CONFIG)
    set (androidaudioplugin-sample-host-engine_LIBS
            android
            binder_ndk
            log
            androidaudioplugin::androidaudioplugin)
else (ANDROID)
    set (androidaudioplugin-sample-host-engine_LIBS
            androidaudioplugin)
endif (ANDROID)
target_link_libraries(androidaudioplugin-sample-host-engine ${androidaudioplugin-sample-host-engine_LIBS})
else ()
target_link_libraries(androidaudioplugin-sample-host-engine android binder_ndk log androidaudioplugin)
endif ()
