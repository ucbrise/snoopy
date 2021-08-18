// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.
#include <openenclave/enclave.h>
#include "../../common/load_balancer_client_enc_pubkey.h"
#include "../../common/empty_server_pubkey.h"
#include "suboram_t.h"
#include "suboram.h"
//#include "oblix/ORAM.hpp"
//#include "oblix/OMAP.h"
//#include "oblix/Bid.h"

#include <sys/socket.h>

typedef struct _enclave_config_data
{
    uint8_t* enclave_secret_data;
    const char* server_pubkey_pem;
    size_t server_pubkey_pem_size;
} enclave_config_data_t;

// For this purpose of this example: demonstrating how to do remote attestation
// g_enclave_secret_data is hardcoded as part of the enclave. In this sample,
// the secret data is hard coded as part of the enclave binary. In a real world
// enclave implementation, secrets are never hard coded in the enclave binary
// since the enclave binary itself is not encrypted. Instead, secrets are
// acquired via provisioning from a service (such as a cloud server) after
// successful attestation.
// The g_enclave_secret_data holds the secret data specific to the holding
// enclave, it's only visible inside this secured enclave. Arbitrary enclave
// specific secret data exchanged by the enclaves. In this sample, the first
// enclave sends its g_enclave_secret_data (encrypted) to the second enclave.
// The second enclave decrypts the received data and adds it to its own
// g_enclave_secret_data, and sends it back to the other enclave.
#define ENCLAVE_SECRET_DATA_SIZE 16

uint8_t g_enclave_secret_data[ENCLAVE_SECRET_DATA_SIZE] =
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

enclave_config_data_t config_data = {g_enclave_secret_data,
                                     SERVER_PUBKEY,
                                     sizeof(SERVER_PUBKEY)};

const char* enclave_name = "SubORAM";

static SuboramDispatcher dispatcher;
//static OMAP *omap;

// Declare a static dispatcher object for enabling
// for better organizing enclave-wise global variables

int set_params(int num_total_blocks, int blocks_per_bucket, int num_balancers, int num_suborams, int suboram_id, int *num_local_blocks, int num_threads) {
    int rv = dispatcher.set_params(num_total_blocks, blocks_per_bucket, num_balancers, num_suborams, suboram_id, num_threads);
    *num_local_blocks = dispatcher.num_local_blocks;
    return rv;
}

int init(void *_s) {
    shared_sort_state<block> *s = static_cast<shared_sort_state<block> *>(_s);
    return dispatcher.init(s);
}

int init_bench_sort(void *_s) {
    shared_sort_state<block_bucket_item> *s = static_cast<shared_sort_state<block_bucket_item> *>(_s);
    return dispatcher.init_bench_sort(s);
}

/*
int init(int num_blocks, int num_balancers, void *_s) {
    shared_sort_state<block_bucket_item> *s = static_cast<shared_sort_state<block_bucket_item> *>(_s);
    return dispatcher.init(num_blocks, num_balancers, s);

    // dispatcher.assign_new_permutation();
    // dispatcher.buffered_bucket_sort();
    // dispatcher.bitonic_sort();
    // return dispatcher.update_block_map();
*/

int oblix_init() {
    /*
    printf("Oblix init\n");
    uint8_t *block_key = (uint8_t *) "0123456789123456"; // TODO: Change keys
    EVP_CIPHER_CTX *suboram_hash_key = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(suboram_hash_key, EVP_aes_128_ecb(), NULL, block_key, NULL);
    
    std::array< uint8_t, 16> value;
    std::fill(value.begin(), value.end(), 0);
    string val((const char *)value.data());
    
    std::map<Bid, string> pairs;
    int depth = (int) (ceil(log2(dispatcher.num_local_blocks)) - 1) + 1;
    long long maxSize = (int) (pow(2, depth));
    for (int i = 1; i < dispatcher.num_total_blocks; i++) {
        if ((get_suboram_for_req(suboram_hash_key, i, dispatcher.num_suborams) == dispatcher.suboram_id)) {
            pairs[i] = val;
        }
    }
    map<unsigned long long, unsigned long long> permutation;
    int j = 0;
    int cnt = 0;
    for (int i = 0; i < maxSize * 4; i++) {
        if (i % 1000000 == 0) {
            printf("%d/%d\n", i, maxSize * 4);
        }
        if (cnt == 4) {
            j++;
            cnt = 0;
        }
        permutation[i] = (j + 1) % maxSize;
        cnt++;
    }
    std::array<byte_t,128> tmp_key{0};
    omap = new OMAP(dispatcher.num_local_blocks, tmp_key, &pairs, &permutation);
    printf("Finished OMAP constructor\n");
    for (int i = 1; i <= dispatcher.num_total_blocks; i++) {
        if (i < dispatcher.num_local_blocks) {
            val = "test_" + std::to_string(i);
            omap->insert(i, val);
            string res = omap->find(i);
            printf("found %d, val: %s\n", i, res.c_str());
        }
    }

    free(suboram_hash_key);
    */
    
    int rv = dispatcher.init(NULL);
    printf("Did dispatcher init\n");
}

    int shuffle_blocks() {
    dispatcher.assign_new_permutation();
    dispatcher.buffered_bucket_sort(0);
    return dispatcher.update_block_map();
}

int buffered_bucket_sort(int thread_idx) {
    return dispatcher.buffered_bucket_sort(thread_idx);
}

int scan_blocks() {
    return dispatcher.scan_blocks();
}

int bitonic_sort() {
    return dispatcher.bitonic_sort();
}

int parallel_bitonic_sort(int thread_id) {
    return dispatcher.parallel_bitonic_sort(thread_id);
}

int parallel_bitonic_sort_nonadaptive(int thread_id) {
    return dispatcher.parallel_bitonic_sort_nonadaptive(thread_id);
}

int verify_sorted() {
    return dispatcher.verify_sorted();
}

int insecure_sort() {
   return dispatcher.insecure_sort(); 
}

int melbourne_sort() {
    return dispatcher.melbourne_sort();
}

void prefetch_bucket_sort() {
    return dispatcher.prefetch_bucket_sort();
}

int bucket_sort() {
   return dispatcher.bucket_sort(); 
}

void process_batch(int *ret, uint8_t *in_ct, uint8_t *in_iv, uint8_t *in_tag, uint8_t *out_ct, uint8_t *out_iv, uint8_t *out_tag, uint32_t batch_sz, int32_t balancer_id) {
    uint32_t *key_arr = (uint32_t *)malloc(batch_sz * sizeof(uint32_t));
    uint8_t **in_data_arr = (uint8_t **)malloc(batch_sz * sizeof(uint8_t *));
    uint8_t **out_data_arr = (uint8_t **)malloc(batch_sz * sizeof(uint8_t *));

    for (int i = 0; i < batch_sz; i++) {
        in_data_arr[i] = (uint8_t *)malloc(BLOCK_LEN);
        out_data_arr[i] = (uint8_t *)malloc(BLOCK_LEN);
    }
    decrypt_key_val_pairs(dispatcher.comm_key[0], key_arr, in_data_arr, batch_sz, in_ct, in_iv, in_tag, &dispatcher.replay_ctr_in[balancer_id], true);

    for (int i = 0; i < batch_sz; i++) {
        dispatcher.process_request(key_arr[i], in_data_arr[i], out_data_arr[i]);
    }
    dispatcher.assign_new_permutation();
    dispatcher.buffered_bucket_sort(0);
    dispatcher.update_block_map();

    encrypt_key_val_pairs(dispatcher.comm_key[0], out_ct, out_iv, out_tag, key_arr, out_data_arr, batch_sz, &dispatcher.replay_ctr_out[balancer_id], true);

    for (int i = 0; i < batch_sz; i++) {
        free(in_data_arr[i]);
        free(out_data_arr[i]);
    }
    free(key_arr);
    free(in_data_arr);
    free(out_data_arr);
}

void table_process_batch_parallel(int *ret, uint8_t *in_ct, uint8_t *in_iv, uint8_t
    *in_tag, uint8_t *out_ct, uint8_t *out_iv, uint8_t *out_tag, uint32_t
    batch_sz, int32_t balancer_id, int thread_id) {
    if (thread_id > 0) {
        *ret = dispatcher.process_requests_thread(thread_id);
        return;
    }
    int rv = ERROR;
    //int num_buckets = 1 << (int) ceil(log2(batch_sz / 4));
    int num_buckets = batch_sz;
    uint32_t *key_arr;
    uint8_t **in_data_arr;
    uint8_t **out_data_arr;
    unsigned char *comm_key = (unsigned char *)"01234567891234567891234567891234";
    CHECK_A (key_arr = (uint32_t *)malloc(batch_sz * sizeof(uint32_t)));
    CHECK_A (in_data_arr = (uint8_t **)malloc(batch_sz * sizeof(uint8_t *)));
    CHECK_A (out_data_arr = (uint8_t **)malloc(batch_sz * sizeof(uint8_t *)));

    for (int i = 0; i < batch_sz; i++) {
        CHECK_A (in_data_arr[i] = (uint8_t *)malloc(BLOCK_LEN));
        CHECK_A (out_data_arr[i] = (uint8_t *)malloc(BLOCK_LEN));
    }
    decrypt_key_val_pairs(comm_key, key_arr, in_data_arr, batch_sz, in_ct, in_iv, in_tag, &dispatcher.replay_ctr_in[balancer_id], true);
    //decrypt_key_val_pairs(dispatcher.comm_key[0], key_arr, in_data_arr, batch_sz, in_ct, in_iv, in_tag, &dispatcher.replay_ctr_in[balancer_id], true);

    dispatcher.process_requests_parallel(batch_sz, num_buckets, key_arr, in_data_arr, out_data_arr);

    CHECK_C (encrypt_key_val_pairs(comm_key, out_ct, out_iv, out_tag, key_arr, out_data_arr, batch_sz, &dispatcher.replay_ctr_out[balancer_id], true));
    //CHECK_C (encrypt_key_val_pairs(dispatcher.comm_key[0], out_ct, out_iv, out_tag, key_arr, out_data_arr, batch_sz, &dispatcher.replay_ctr_out[balancer_id], true));

    cleanup:
    for (int i = 0; i < batch_sz; i++) {
        if (in_data_arr && in_data_arr[i])
            free(in_data_arr[i]);
        if (out_data_arr && out_data_arr[i])
            free(out_data_arr[i]);
    }
    if (key_arr) free(key_arr);
    if (in_data_arr) free(in_data_arr);
    if (out_data_arr) free(out_data_arr);
    *ret = rv;
}


void oblix_process_batch(int *ret, uint8_t *in_ct, uint8_t *in_iv, uint8_t *in_tag, uint8_t *out_ct, uint8_t *out_iv, uint8_t *out_tag, uint32_t batch_sz, int32_t balancer_id) {
    uint32_t *key_arr = (uint32_t *)malloc(batch_sz * sizeof(uint32_t));
    uint8_t **in_data_arr = (uint8_t **)malloc(batch_sz * sizeof(uint8_t *));
    //uint8_t **out_data_arr = (uint8_t **)malloc(batch_sz * sizeof(uint8_t *));

    for (int i = 0; i < batch_sz; i++) {
        in_data_arr[i] = (uint8_t *)malloc(BLOCK_LEN);
        //out_data_arr[i] = (uint8_t *)malloc(BLOCK_LEN);
    }
    decrypt_key_val_pairs(dispatcher.comm_key[0], key_arr, in_data_arr, batch_sz, in_ct, in_iv, in_tag, &dispatcher.replay_ctr_in[balancer_id], true);

    /*
    // TODO: oblix process requests
    uint8_t zeros[BLOCK_LEN];
    memset(zeros, 0, BLOCK_LEN);
    std::array< uint8_t, 16> value;
    std::fill(value.begin(), value.end(), 0);
    for (int i = 0; i < batch_sz; i++) {
        printf("Processing batch element %d/%d\n", i, batch_sz);
        // This isn't completely secure but is the only API omix provides
        if (memcmp(in_data_arr[i], zeros, BLOCK_LEN) == 0) {
            // Read
            string res = omap->find(key_arr[i]);
            memcpy(in_data_arr[i], res.c_str(), BLOCK_LEN);
        } else {
            // Write
            for (int j = 0; j < BLOCK_LEN; j++) {
                value[j] = in_data_arr[i][j];
            }
            //std::copy_n(std::begin(in_data_arr[i]), BLOCK_LEN, value.begin());
            string val((const char *)value.data());

            omap->insert(key_arr[i], val);
        }
    }
    printf("Done with batch\n");
    */

    encrypt_key_val_pairs(dispatcher.comm_key[0], out_ct, out_iv, out_tag, key_arr, in_data_arr, batch_sz, &dispatcher.replay_ctr_out[balancer_id], true);

    for (int i = 0; i < batch_sz; i++) {
        free(in_data_arr[i]);
        //free(out_data_arr[i]);
    }
    free(key_arr);
    free(in_data_arr);
    //free(out_data_arr);
}


