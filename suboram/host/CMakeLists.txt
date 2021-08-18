# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

add_custom_command(
  OUTPUT suboram_u.h suboram_u.c suboram_args.h
  DEPENDS ${CMAKE_SOURCE_DIR}/suboram/suboram.edl
  COMMAND
    openenclave::oeedger8r --untrusted
    ${CMAKE_SOURCE_DIR}/suboram/suboram.edl --search-path ${OE_INCLUDEDIR}
    --search-path ${OE_INCLUDEDIR}/openenclave/edl/sgx)

add_executable(suboram_host host.cpp ../../common/crypto.cpp
  ../../common/block.cpp
  ../enc/suboram.cpp
  ../../common/bucket_sort.cpp
  ../../common/par_obl_primitives.cpp
  oblix/RAMStore.cpp oblix/Utilities.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/suboram_u.c)

include_directories(${PROTOBUF_INCLUDE_DIRS} ${PROTO_GRPC_DIR})

target_include_directories(suboram_host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                                   ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(suboram_host
    PRIVATE log
    openenclave::oehost
    oram_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
