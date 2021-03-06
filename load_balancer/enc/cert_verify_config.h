// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef ATTLESTED_TLS_ENCLAVE_CONFIG
#define ATTLESTED_TLS_ENCLAVE_CONFIG

#include <openenclave/enclave.h>
#include <stdio.h>
#include "../../common/common.h"
#include "../../common/tls_client_enc_pubkey.h"
#include "../../common/suboram_enc_pubkey.h"
#include "../../common/suboram_enc_mrenclave.h"
#define TLS_ENCLAVE TLS_SERVER

oe_result_t verify_claim_value(const oe_claim_t* claim);

#endif
