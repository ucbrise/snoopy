# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

# Use the edger8r to generate C bindings from the EDL file.
add_custom_command(
  OUTPUT load_balancer_t.h load_balancer_t.c load_balancer_args.h
  DEPENDS ${CMAKE_SOURCE_DIR}/load_balancer/load_balancer.edl
  COMMAND
    openenclave::oeedger8r --trusted ${CMAKE_SOURCE_DIR}/load_balancer/load_balancer.edl
    --search-path ${OE_INCLUDEDIR} --search-path
    ${OE_INCLUDEDIR}/openenclave/edl/sgx)

# Sign enclave and generate a header file for server's MRENCLAVE
add_custom_command(
  OUTPUT load_balancer_enc.signed load_balancer_enc_mrenclave.h
  DEPENDS load_balancer_enc enc.conf ${CMAKE_SOURCE_DIR}/load_balancer/enc/private.pem
          ${CMAKE_SOURCE_DIR}/gen_mrenclave_header.sh
  COMMAND
    openenclave::oesign sign -e $<TARGET_FILE:load_balancer_enc> -c
    ${CMAKE_SOURCE_DIR}/load_balancer/enc/enc.conf -k
    ${CMAKE_SOURCE_DIR}/load_balancer/enc/private.pem
  COMMAND openenclave::oesign dump -e load_balancer_enc.signed > temp.dmp
  COMMAND bash ${CMAKE_SOURCE_DIR}/gen_mrenclave_header.sh
          ${CMAKE_SOURCE_DIR}/common/load_balancer_enc_mrenclave.h temp.dmp
  COMMAND ${CMAKE_COMMAND} -E sleep 1
  COMMAND ${CMAKE_COMMAND} -E remove temp.dmp)

add_executable(
    load_balancer_enc
    ecalls.cpp
    openssl_server.cpp
    balancer.cpp
    test_balancer.cpp
    cert_verify_config.cpp
    obl_primitives.cpp
    ../../common/verify_callback.cpp
    ../../common/utility.cpp
    ../../common/openssl_utility.cpp
    ../../common/crypto.cpp
    ../../common/block.cpp
    ../../common/bucket_sort.cpp
    ../../common/par_obl_primitives.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/load_balancer_t.c)

if (WIN32)
  maybe_build_using_clangw(load_balancer_enc)
endif ()

target_compile_definitions(load_balancer_enc PUBLIC OE_API_VERSION=2)

target_include_directories(
  load_balancer_enc
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
          ${CMAKE_BINARY_DIR}/load_balancer/enc)

target_link_libraries(
    load_balancer_enc PRIVATE log_enc openenclave::oelibcxx openenclave::oeenclave openenclave::oecryptoopenssl
    openenclave::oehostsock openenclave::oehostresolver)

add_custom_target(load_balancer_sign_enc ALL DEPENDS load_balancer_enc.signed
                                                  load_balancer_enc_mrenclave.h)

add_executable(test_balancer
    balancer.cpp
    test_balancer.cpp
    ../../common/crypto.cpp
    ../../common/block.cpp
    ../../common/bucket_sort.cpp
    ../../common/par_obl_primitives.cpp
)
target_compile_definitions(test_balancer PUBLIC OE_API_VERSION=2)
target_include_directories(
  test_balancer 
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
          ${CMAKE_BINARY_DIR}/load_balancer/enc)
target_link_libraries(
    test_balancer PRIVATE
    ${OPENSSL_LIBRARIES}
    #log_enc
    #openenclave::oelibcxx openenclave::oeenclave
    #openenclave::oecryptoopenssl
    #openenclave::oehostsock openenclave::oehostresolver
)
add_test_deps(test_balancer)
add_test(NAME test_balancer COMMAND test_balancer)