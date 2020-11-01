cmake_minimum_required(VERSION 3.13.0)

add_executable(aap-list
        "aap-list.cpp"
        )

target_include_directories(aap-list
        PRIVATE
        "../../plugin-api/include"
        "../../androidaudioplugin/core/include"
        )

target_compile_options(aap-list
        PRIVATE
        -std=c++17
        -Wall
        -Wshadow
        )

target_link_directories(aap-list
        PRIVATE
	"../build/native/androidaudioplugin/"
)

target_link_libraries(aap-list ${aap-list-LIBS} androidaudioplugin)

