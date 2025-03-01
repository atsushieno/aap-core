cmake_minimum_required(VERSION 3.6.0)

project(aappluginsamples LANGUAGES C CXX)

find_package (androidaudioplugin REQUIRED CONFIG)

add_library(aap-instrument-sample SHARED
        "instrument/aap-instrument-sample.cpp"
        "instrument/ayumi.c"
)
add_library(aap-effect-sample SHARED
        "effect/aap-effect-sample.cpp"
)
add_library(aap-xs-sample SHARED
        "aapxs/test-extension-service.cpp"
        "aapxs/aap-xs-sample.cpp"
)
target_include_directories(aap-xs-sample PRIVATE
        "aapxs/include"
)
list(APPEND target-plugins aap-instrument-sample aap-effect-sample aap-xs-sample)

foreach(TheTarget ${target-plugins})
    message("TheTarget: ${TheTarget}")

    target_include_directories(${TheTarget} PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/../../../../../include"
            "${CMAKE_CURRENT_LIST_DIR}/../../../../../external/cmidi2"
    )

    target_compile_options(${TheTarget} PRIVATE
            -Wall
            -Wextra-semi
            -Wshadow
            -Wshadow-field
    )

    target_link_libraries(${TheTarget} PRIVATE
            android
            log
            androidaudioplugin::androidaudioplugin
    )

    # You can set it via build.gradle
    if (${AAP_ENABLE_ASAN})
        target_compile_options (${TheTarget} PRIVATE
                -fsanitize=address -fno-omit-frame-pointer
        )
        target_link_options(${TheTarget} PRIVATE
                -fsanitize=address
        )
        target_set_properties(${TheTarget} PRIVATE
                LINK_FLAGS -fsanitize=address
        )
    endif()
endforeach ()
