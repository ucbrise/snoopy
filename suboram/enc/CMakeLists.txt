# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.
# Use the edger8r to generate C bindings from the EDL file.
add_custom_command(
  OUTPUT suboram_t.h suboram_t.c suboram_args.h
  DEPENDS ${CMAKE_SOURCE_DIR}/suboram/suboram.edl
  COMMAND
    openenclave::oeedger8r --trusted ${CMAKE_SOURCE_DIR}/suboram/suboram.edl
    --search-path ${OE_INCLUDEDIR} --search-path
    ${OE_INCLUDEDIR}/openenclave/edl/sgx)
  
add_custom_target(suboram_hdrs DEPENDS suboram_t.h suboram_t.c suboram_args.h)

#add_subdirectory(oblix)


# Sign enclave and generate a header file for server's MRENCLAVE
add_custom_command(
  OUTPUT suboram_enc.signed suboram_enc_mrenclave.h
  DEPENDS suboram_enc enc.conf ${CMAKE_SOURCE_DIR}/suboram/enc/private.pem
          ${CMAKE_SOURCE_DIR}/gen_mrenclave_header.sh
  COMMAND
    openenclave::oesign sign -e $<TARGET_FILE:suboram_enc> -c
    ${CMAKE_SOURCE_DIR}/suboram/enc/enc.conf -k
    ${CMAKE_SOURCE_DIR}/suboram/enc/private.pem
  COMMAND openenclave::oesign dump -e suboram_enc.signed > temp.dmp
  COMMAND bash ${CMAKE_SOURCE_DIR}/gen_mrenclave_header.sh
          ${CMAKE_SOURCE_DIR}/common/suboram_enc_mrenclave.h temp.dmp
  COMMAND ${CMAKE_COMMAND} -E sleep 1
  COMMAND ${CMAKE_COMMAND} -E remove temp.dmp)

add_executable(
    suboram_enc
    suboram.cpp
    ecalls.cpp
    cert_verify_config.cpp
    ../../common/verify_callback.cpp
    ../../common/utility.cpp
    ../../common/openssl_utility.cpp
    ../../common/crypto.cpp
    ../../common/block.cpp
    ../../common/bucket_sort.cpp
    ../../common/par_obl_primitives.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/suboram_t.c)

if (WIN32)
  maybe_build_using_clangw(suboram_enc)
endif ()

target_compile_definitions(suboram_enc PUBLIC OE_API_VERSION=2)

target_include_directories(
  suboram_enc
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
          ${CMAKE_BINARY_DIR}/suboram/enc)

target_link_libraries(
    suboram_enc log_enc openenclave::oeenclave openenclave::oecryptoopenssl
    openenclave::oelibcxx openenclave::oehostsock openenclave::oehostresolver)

add_custom_target(suboram_sign_enc ALL DEPENDS suboram_enc.signed
                                                  suboram_enc_mrenclave.h)
