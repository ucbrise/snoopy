set(PROTOS
    ${CMAKE_CURRENT_SOURCE_DIR}/protos/oram.proto
)
set(PROTOS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/protos")
set(PROTO_HDRS "${CMAKE_CURRENT_BINARY_DIR}/oram.pb.h")
set(PROTO_SRCS "${CMAKE_CURRENT_BINARY_DIR}/oram.pb.cc")
set(GRPC_SRCS "${CMAKE_CURRENT_BINARY_DIR}/oram.grpc.pb.cc")
set(GRPC_HDRS "${CMAKE_CURRENT_BINARY_DIR}/oram.grpc.pb.h")
add_custom_command(
      OUTPUT "${PROTO_SRCS}" "${PROTO_HDRS}" "${GRPC_SRCS}" "${GRPC_HDRS}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${PROTOS_PATH}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${PROTOS}"
      DEPENDS "${PROTOS}")
set(PROTO_SRCS ${PROTO_SRCS} PARENT_SCOPE)
set(PROTO_HDRS ${PROTO_HDRS} PARENT_SCOPE)
set(GRPC_SRCS ${GRPC_SRCS} PARENT_SCOPE)
set(GRPC_HDRS ${GRPC_HDRS} PARENT_SCOPE)
set(PROTO_GRPC_DIR "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)
add_custom_target(protos DEPENDS ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_HDRS} ${GRPC_SRCS})

include_directories("${CMAKE_CURRENT_BINARY_DIR}")
add_library(oram_grpc_proto
    ${PROTO_SRCS}
    ${PROTO_HDRS}
    ${GRPC_HDRS}
    ${GRPC_SRCS})

target_link_libraries(oram_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})


add_executable(test_ring_buffer test_ring_buffer.cpp)
add_test_deps(test_ring_buffer)
add_test(NAME test_ring_buffer COMMAND test_ring_buffer)
