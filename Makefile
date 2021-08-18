# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.11)

project("Attested TLS sample" LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 11)

find_package(OpenEnclave CONFIG REQUIRED)

add_subdirectory(load_balancer)
add_subdirectory(client)
add_subdirectory(suboram)
add_subdirectory(oblix)

if ((NOT DEFINED ENV{OE_SIMULATION}) OR (NOT $ENV{OE_SIMULATION}))
  add_custom_target(
    run
    DEPENDS suboram client suboram_enc 
#    COMMAND ${CMAKE_COMMAND} -E sleep 2
#    COMMENT
#      "Launch processes to establish an Attested TLS between an non-enclave TLS client and an TLS server running inside an enclave "
    COMMAND
      bash -c
	  "${CMAKE_BINARY_DIR}/suboram/host/suboram_host ${CMAKE_BINARY_DIR}/suboram/enc/suboram_enc.signed -port:12346 -num_blocks:128 -server-in-loop &"
#    COMMAND ${CMAKE_COMMAND} -E sleep 2
    #COMMAND
    #  bash -c
    #  "${CMAKE_BINARY_DIR}/load_balancer/host/load_balancer_host ${CMAKE_BINARY_DIR}/load_balancer/enc/load_balancer_enc.signed -port:12345 -suboram_name:localhost -suboram_port:12346 -server-in-loop &"
    #COMMAND ${CMAKE_COMMAND} -E sleep 2
#    COMMAND ${CMAKE_BINARY_DIR}/client/client
#            -server:localhost -port:12345)
endif ()
