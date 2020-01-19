cmake_minimum_required(VERSION 3.5.1)

project (aap-import-lv2-metadata)
add_executable(aap-import-lv2-metadata aap-import-lv2-metadata.cpp)

set (aap-import-lv2-metadata_INCLUDES
        "../../dependencies/lv2-desktop/dist/include/lilv-0"
        "../../dependencies/lv2-desktop/dist/include"
        )

set (ENV{PKG_CONFIG_PATH} "../../dependencies/lv2-desktop/dist/lib/pkgconfig/")

find_package ( PkgConfig REQUIRED )

pkg_check_modules ( SERD REQUIRED serd-0>=0.28.0 )
pkg_check_modules ( SORD REQUIRED sord-0>=0.16.0 )
pkg_check_modules ( SRATOM REQUIRED sratom-0>=0.6.0 )
pkg_check_modules ( LILV REQUIRED lilv-0>=0.24.0 )

target_link_libraries(aap-import-lv2-metadata ${SERD_LIBRARIES})
target_include_directories(aap-import-lv2-metadata PUBLIC ${SERD_INCLUDE_DIRS})
target_compile_options(aap-import-lv2-metadata PUBLIC ${SERD_CFLAGS_OTHER})
