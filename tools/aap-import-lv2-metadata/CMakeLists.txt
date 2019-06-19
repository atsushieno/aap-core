cmake_minimum_required (VERSION 2.6)
find_package(PkgConfig)
project (aap-import-lv2-metadata)
add_executable(aap-import-lv2-metadata aap-import-lv2-metadata.cpp)

pkg_check_modules(SERD REQUIRED serd-0)
pkg_check_modules(SERD REQUIRED sord-0)
pkg_check_modules(SERD REQUIRED lilv-0)

target_link_libraries(aap-import-lv2-metadata ${SERD_LIBRARIES})
target_include_directories(aap-import-lv2-metadata PUBLIC ${SERD_INCLUDE_DIRS})
target_compile_options(aap-import-lv2-metadata PUBLIC ${SERD_CFLAGS_OTHER})
