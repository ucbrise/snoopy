# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

#include(../cmake/common.cmake)

find_package(OpenSSL REQUIRED)

add_executable(client client.cpp cert_verify_config.cpp ../common/crypto.cpp
                                  ../common/verify_callback.cpp ../common/block.cpp)

add_dependencies(client load_balancer_sign_enc)

include_directories(${PROTOBUF_INCLUDE_DIRS} ${PROTO_GRPC_DIR} ${BOOST_DIR})

target_include_directories(
  client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                 ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(client
    openenclave::oehost
    OpenSSL::SSL
    log
    oram_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})

add_dependencies(client load_balancer)
