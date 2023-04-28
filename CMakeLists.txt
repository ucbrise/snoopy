# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.11)

project("Scalable ORAM" LANGUAGES C CXX)

enable_testing()
set(TEST_DEPS "")

set(CMAKE_CXX_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -march=native")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_DEBUG "-gstabs -gstabs+")

# set OE_CRYPTO_LIB to either "mbedtls" or "openssl" based on the crypto wrapper to be used.
# OE_CRYPTO_LIB is case sensitive. Use all lowercase letters.

find_package(OpenEnclave CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

# GRPC and Protocol Buffers libraries location
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/third_party/protobuf/install" "${CMAKE_SOURCE_DIR}/third_party/grpc/install")

# Cmake find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
message(${CMAKE_MODULE_PATH})
find_package(Protobuf REQUIRED)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
find_package(gRPC CONFIG REQUIRED)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
set(_GRPC_GRPCPP gRPC::grpc++)
set(_REFLECTION gRPC::grpc++_reflection)
set(BOOST_DIR /usr/local/boost_1_75_0)

include_directories(include)

# convenience macro for adding test dependencies
macro (add_test_deps)
  file (RELATIVE_PATH _relPath "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  foreach (_dep ${ARGN})
    list(APPEND TEST_DEPS ${_dep})
  endforeach()
  if (_relPath)
    set (TEST_DEPS ${TEST_DEPS} PARENT_SCOPE)
  endif()
endmacro()

add_definitions( -DUSE_AVX2=1 )

add_subdirectory(common)
message(${PROTO_SRCS})
add_subdirectory(common/log)
include_directories(common/log)
add_subdirectory(client)
add_subdirectory(load_balancer)
add_subdirectory(suboram)

if ((NOT DEFINED ENV{OE_SIMULATION}) OR (NOT $ENV{OE_SIMULATION}))
  add_custom_target(
    run
    DEPENDS protos client suboram suboram_enc
    #    COMMAND ${CMAKE_COMMAND} -E sleep 2
    #COMMENT
    #  "Launch processes to establish an Attested TLS between an non-enclave TLS client and an TLS server running inside an enclave "
    COMMAND
      bash -c
      "${CMAKE_BINARY_DIR}/suboram/host/suboram_host ${CMAKE_BINARY_DIR}/suboram/enc/suboram_enc.signed -port:12346 -num_blocks:1024 -server-in-loop"
    #COMMAND ${CMAKE_COMMAND} -E sleep 2
    #COMMAND
    #  bash -c
    #  "${CMAKE_BINARY_DIR}/load_balancer/host/load_balancer_host ${CMAKE_BINARY_DIR}/load_balancer/enc/load_balancer_enc.signed -port:12345 -suboram_name:localhost -suboram_port:12346 -server-in-loop &"
    #  COMMAND ${CMAKE_COMMAND} -E sleep 2)
      # COMMAND ${CMAKE_BINARY_DIR}/client/client
      #      -server:localhost -port:12345)
    )
endif ()

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose DEPENDS ${TEST_DEPS})
