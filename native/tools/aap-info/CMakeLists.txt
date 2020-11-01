cmake_minimum_required(VERSION 3.13.0)

add_executable(aap-info
        "aap-info.cpp"
        )

target_include_directories(aap-info
        PRIVATE
        "../../plugin-api/include"
        "../../androidaudioplugin/core/include"
        )

target_compile_options(aap-info
        PRIVATE
        -std=c++17
        -Wall
        -Wshadow
        )

target_link_directories(aap-info
        PRIVATE
	"../build/native/androidaudioplugin/"
)

target_link_libraries(aap-info ${aap-info-LIBS} androidaudioplugin)

