// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef ATTLESTED_TLS_ENCLAVE_CONFIG
#define ATTLESTED_TLS_ENCLAVE_CONFIG

#include <openenclave/host.h>
#include <stdio.h>
#include "../common/common.h"
#include "../common/load_balancer_enc_mrenclave.h"
#include "../common/load_balancer_server_enc_pubkey.h"
#include "../common/empty_client_pubkey.h"
#define TLS_ENCLAVE TLS_CLIENT

oe_result_t verify_claim_value(const oe_claim_t* claim);

#endif
