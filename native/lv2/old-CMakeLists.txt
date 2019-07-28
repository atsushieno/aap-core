cmake_minimum_required(VERSION 3.4.1)

find_package ( PkgConfig REQUIRED )

# Mandatory libraries: serd, sord, lilv
pkg_check_modules ( SERD REQUIRED serd-0>=0.28.0 )
pkg_check_modules ( SORD REQUIRED sord-0>=0.16.0 )
pkg_check_modules ( SRATOM REQUIRED sratom-0>=0.6.0 )
pkg_check_modules ( LILV REQUIRED lilv-0>=0.24.0 )

set (aap-lv2_sources
	src/android-audio-plugin-lv2-bridge.cpp)

add_library(aap-lv2 ${aap-lv2_sources})

target_include_directories(aap-lv2
	PRIVATE src ../common/include
	${SERD_INCLUDE_DIRS}
	${SORD_INCLUDE_DIRS}
	${SRATOM_INCLUDE_DIRS}
	${LILV_INCLUDE_DIRS}
	PUBLIC include)

target_compile_options(aap-lv2
	PRIVATE
	-std=c++14
	-Wall
	-Wextra-semi
	-Wshadow
	-Ofast
	"$<$<CONFIG:DEBUG>:-Werror>")

# target_compile_definitions(aap-lv2 PUBLIC "$<$<CONFIG:DEBUG>:ANY_ADDITIONAL_DEFS>")

target_link_libraries(aap-lv2 PRIVATE ${SERD_LIBRARIES} ${SORD_LIBRARIES} ${SRATOM_LIBRARIES} ${LILV_LIBRARIES} )
