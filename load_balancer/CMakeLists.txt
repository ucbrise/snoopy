# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

add_subdirectory(enc)
add_subdirectory(host)

add_custom_target(load_balancer DEPENDS load_balancer_host load_balancer_enc
                                     load_balancer_sign_enc)
