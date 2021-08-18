# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

add_subdirectory(enc)
add_subdirectory(host)

add_custom_target(suboram DEPENDS suboram_host suboram_enc
                                     suboram_sign_enc)
