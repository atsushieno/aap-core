cmake_minimum_required(VERSION 3.5.1)

project(androidaudioplugin VERSION 0.6.2 LANGUAGES CXX)

# List of sources. Android build has some additional sources.
set (androidaudioplugin_SOURCES
	"core/src/android-audio-plugin-host.cpp"
	"../../dependencies/tinyxml2/tinyxml2.cpp"
	)

set (androidaudioplugin_INCLUDES
	"../plugin-api/include"
	"core/include"
	"android/include"
	"android/src/gen/include"
	"../../dependencies/tinyxml2"
	)

if (ANDROID)

set (androidaudioplugin_SOURCES
	${androidaudioplugin_SOURCES}
	"android/src/android-audio-plugin-client-bridge.cpp"
	"android/src/gen/org/androidaudioplugin/AudioPluginInterface.cpp"
	"android/src/android-context.cpp"
	"android/src/AudioPluginHost_native.cpp"
	"android/src/audio-plugin-host-android.cpp"
	)

set (androidaudioplugin_LIBS ${androidaudioplugin_LIBS} android log binder_ndk)

else (ANDROID)

# copypasting from https://stackoverflow.com/questions/32823563/using-cmake-to-create-protobuf-grpc-cc-files
# (Tried PLUGIN argument too but it didn't work)
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin) # Get full path to plugin

function(PROTOBUF_GENERATE_GRPC_CPP SRCS HDRS)
  if(NOT ARGN)
    return()
  endif()

  if(PROTOBUF_GENERATE_CPP_APPEND_PATH) # This variable is common for all types of output.
    # Create an include path for each file specified
    foreach(FIL ${ARGN})
      get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
      get_filename_component(ABS_PATH ${ABS_FIL} PATH)
      list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
      if(${_contains_already} EQUAL -1)
          list(APPEND _protobuf_include_path -I ${ABS_PATH})
      endif()
    endforeach()
  else()
    set(_protobuf_include_path -I ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  if(DEFINED PROTOBUF_IMPORT_DIRS)
    foreach(DIR ${Protobuf_IMPORT_DIRS})
      get_filename_component(ABS_PATH ${DIR} ABSOLUTE)
      list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
      if(${_contains_already} EQUAL -1)
          list(APPEND _protobuf_include_path -I ${ABS_PATH})
      endif()
    endforeach()
  endif()

  set(${SRCS})
  set(${HDRS})
  foreach(FIL ${ARGN})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    get_filename_component(FIL_WE ${FIL} NAME_WE)

    list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.cc")
    list(APPEND ${HDRS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.h")

    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.cc"
             "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.h"
      COMMAND  ${Protobuf_PROTOC_EXECUTABLE}
      ARGS --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
           --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
           ${_protobuf_include_path} ${ABS_FIL}
      DEPENDS ${ABS_FIL} ${Protobuf_PROTOC_EXECUTABLE}
      COMMENT "Running gRPC C++ protocol buffer compiler on ${FIL}"
      VERBATIM)
  endforeach()

  set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
  set(${SRCS} ${${SRCS}} PARENT_SCOPE)
  set(${HDRS} ${${HDRS}} PARENT_SCOPE)
endfunction()
#end of copypasting

PROTOBUF_GENERATE_CPP(GenProtoSources GenProtoIncludes "desktop/src/AudioPluginService.proto")
PROTOBUF_GENERATE_GRPC_CPP(GenProtoGrpcSources GenProtoGrpcIncludes "desktop/src/AudioPluginService.proto")
list(GET GenProtoIncludes 0 GenProtoInclude0)
list(GET GenProtoGrpcIncludes 0 GenProtoGrpcInclude0)
get_filename_component(GEN_PROTO_INCLUDE_DIR "${GenProtoInclude0}" DIRECTORY)
get_filename_component(GEN_PROTO_GRPC_INCLUDE_DIR "${GenProtoGrpcInclude0}" DIRECTORY)

set (androidaudioplugin_SOURCES
	${androidaudioplugin_SOURCES}
	"desktop/src/desktop-audio-plugin-client-bridge.cpp"
	${GenProtoSources}
	${GenProtoGrpcSources}
	"desktop/src/audio-plugin-host-desktop.cpp"
	"desktop/src/AudioPluginInterfaceImpl.cpp"
	)

set (androidaudioplugin_INCLUDES
	${androidaudioplugin_INCLUDES}
	${GEN_PROTO_INCLUDE_DIR}
	${GEN_PROTO_GRPC_INCLUDE_DIR}
	${PROTOBUF_INCLUDE_DIRS}
	${GRPCPP_INCLUDE_DIRS}
	)

set (androidaudioplugin_LIBRARY_DIRS ${PROTOBUF_LIBRARY_DIRS} ${GRPC_LIBRARY_DIRS} ${GRPCPP_LIBRARY_DIRS})

set (androidaudioplugin_LIBS ${androidaudioplugin_LIBS} ${PROTOBUF_LIBRARIES} ${GRPC_LIBRARIES} ${GRPCPP_LIBRARIES} dl)

find_library(LIBRT "rt")
if (NOT "${LIBRT}" MATCHES LIBRT-NOTFOUND)
set (androidaudioplugin_LIBS ${androidaudioplugin_LIBS} rt)
endif ()

endif (ANDROID)


add_library ( # Specifies the name of the library.
            androidaudioplugin

            # Sets the library as a shared library.
            SHARED

            # Provides a relative path to your source file(s).
            ${androidaudioplugin_SOURCES}
            )

target_include_directories (androidaudioplugin
        PRIVATE
        ${androidaudioplugin_INCLUDES}
        )

target_compile_options (androidaudioplugin
        PRIVATE
        -std=c++17
        -Wall
        -Wshadow
        )

target_link_directories(androidaudioplugin
		PRIVATE
		${PROTOBUF_LIBRARY_DIRS}
		${GRPCPP_LIBRARY_DIRS}
		)

target_link_libraries (androidaudioplugin
		${androidaudioplugin_LIBS}
		)
