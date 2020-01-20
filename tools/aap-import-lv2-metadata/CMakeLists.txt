cmake_minimum_required(VERSION 3.5.1)

project (aap-import-lv2-metadata)

link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../dependencies/lv2-desktop/dist/lib")

add_executable(aap-import-lv2-metadata aap-import-lv2-metadata.cpp)

set (aap-import-lv2-metadata_INCLUDES
        "../../dependencies/lv2-desktop/dist/include/lilv-0"
        "../../dependencies/lv2-desktop/dist/include"
        )
set (ENV{PKG_CONFIG_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../../dependencies/lv2-desktop/dist/lib/pkgconfig/")

find_package ( PkgConfig REQUIRED )

pkg_check_modules ( SERD REQUIRED IMPORTED_TARGET serd-0>=0.28.0 )
pkg_check_modules ( SORD REQUIRED IMPORTED_TARGET sord-0>=0.16.0 )
pkg_check_modules ( SRATOM REQUIRED IMPORTED_TARGET sratom-0>=0.6.0 )
pkg_check_modules ( LILV REQUIRED IMPORTED_TARGET lilv-0>=0.24.0 )

target_link_libraries(aap-import-lv2-metadata ${LILV_LIBRARIES})
target_include_directories(aap-import-lv2-metadata PUBLIC ${SERD_INCLUDE_DIRS} ${SORD_INCLUDE_DIRS} ${LILV_INCLUDE_DIRS})
target_compile_options(aap-import-lv2-metadata PUBLIC ${SERD_CFLAGS_OTHER}
        PRIVATE
        -std=c++17
        -Wall
        -Wshadow
        )
