# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

add_custom_command(
  OUTPUT load_balancer_u.h load_balancer_u.c load_balancer_args.h
  DEPENDS ${CMAKE_SOURCE_DIR}/load_balancer/load_balancer.edl
  COMMAND
    openenclave::oeedger8r --untrusted
    ${CMAKE_SOURCE_DIR}/load_balancer/load_balancer.edl --search-path ${OE_INCLUDEDIR}
    --search-path ${OE_INCLUDEDIR}/openenclave/edl/sgx)

add_executable(load_balancer_host host.cpp
                ../../common/crypto.cpp
                ../../common/block.cpp
                ../../common/bucket_sort.cpp
                ${CMAKE_CURRENT_BINARY_DIR}/load_balancer_u.c)

include_directories(${PROTOBUF_INCLUDE_DIRS} ${PROTO_GRPC_DIR} ${BOOST_DIR})

target_include_directories(load_balancer_host 
                            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(load_balancer_host
    PRIVATE log
    openenclave::oehost
    oram_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
