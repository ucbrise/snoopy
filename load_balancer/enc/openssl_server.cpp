// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include <errno.h>
#include <arpa/inet.h>
#include <openenclave/enclave.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "log.h"
#include "balancer.h"

#include "../../common/openssl_utility.h"
#include "../../common/obl_primitives.h"
#include "../../common/block.h"

extern "C"
{
    void init_load_balancer(int num_suborams_, int num_blocks_, int num_threads);
    void create_batch(uint8_t **in_ct_arr, uint8_t **in_iv_arr, uint8_t
            **in_tag_arr, uint32_t *in_client_id_arr, uint8_t **out_ct_arr, uint8_t
            **out_iv_arr, uint8_t **out_tag_arr, int num_reqs, int reqs_per_suboram);
    void match_responses_to_reqs(uint8_t **in_ct_arr, uint8_t **in_iv_arr, uint8_t **in_tag_arr, uint32_t *in_client_id_arr, uint8_t **out_ct_arr, uint8_t **out_iv_arr, uint8_t **out_tag_arr);
    void start_worker_loop(int thread_id);
    void stop_workers();
};

LoadBalancer *lb;
Batch *batch;
int num_suborams;

void init_load_balancer(int num_suborams_, int num_blocks_, int num_threads) {
    num_suborams = num_suborams_;
    lb = new LoadBalancer(num_suborams, num_blocks_, num_threads);
}

void start_worker_loop(int thread_id) {
    lb->worker_loop(thread_id);
}

void stop_workers() {
    lb->stop_workers();
}

// TODO: need this all to be actually encrypted before goes out of enclave
// TODO: attach to each request a tag corresponding to the incoming index, then
// when get responses back from suborams sort by this tag so that know how to
// send responses back to clients.
void create_batch(uint8_t **in_ct_arr, uint8_t **in_iv_arr, uint8_t
        **in_tag_arr, uint32_t *in_client_id_arr, uint8_t **out_ct_arr, uint8_t
        **out_iv_arr, uint8_t **out_tag_arr, int num_reqs, int reqs_per_suboram) {
    debug_log::set_name("lb_enc");
    int ret = 0;

    batch = new Batch(reqs_per_suboram);
    uint8_t buf[BLOCK_LEN];
    uint32_t key;
    set<uint32_t>client_ids;

    for (int i = 0; i < num_reqs; i++) {
        if (lb->client_replay_ctr_in.find(in_client_id_arr[i]) == lb->client_replay_ctr_in.end()) {
            lb->client_replay_ctr_in[in_client_id_arr[i]] = 0;
            lb->client_replay_ctr_out[in_client_id_arr[i]] = 1;
        }
        decrypt_key_val(lb->client_comm_key[0], &key, buf, in_ct_arr[i], in_iv_arr[i], in_tag_arr[i], &lb->client_replay_ctr_in[in_client_id_arr[i]], false);
        client_ids.insert(in_client_id_arr[i]);
        //decrypt_key_val(lb->client_comm_key[in_client_id_arr[i]], &key, buf, in_ct_arr[i], in_iv_arr[i], in_tag_arr[i], &lb->client_replay_ctr_in[in_client_id_arr[i]]);
        KeyBlockPairBucketItem req(i, key, buf, in_client_id_arr[i]);
        batch->add_incoming_request(req);
    }
    set<uint32_t>::iterator it = client_ids.begin();
    while (it != client_ids.end()) {
        lb->client_replay_ctr_in[*it] = lb->client_replay_ctr_in[*it] + 1;
        it++;
    }
    lb->create_outgoing_batch(*batch);

    uint32_t *key_arr = (uint32_t *)malloc(reqs_per_suboram * sizeof(uint32_t));
    uint8_t **block_arr = (uint8_t **)malloc(reqs_per_suboram * sizeof(uint8_t *));
    size_t ctr = 0;

    for (int i = 0; i < lb->num_suborams; i++) {
        for (int j = 0; j < reqs_per_suboram; j++) {
            key_arr[j] = batch->outgoing_reqs[ctr].item.req.key;
            block_arr[j] = batch->outgoing_reqs[ctr].item.req.block;
            ctr++;
        }
        encrypt_key_val_pairs(lb->suboram_comm_key[0], out_ct_arr[i], out_iv_arr[i], out_tag_arr[i], key_arr, block_arr, reqs_per_suboram, &lb->suboram_replay_ctr_out[i], true);
    }

    free(key_arr);
    free(block_arr);
}


void match_responses_to_reqs(uint8_t **in_ct_arr, uint8_t **in_iv_arr, uint8_t **in_tag_arr, uint32_t *in_client_id_arr, uint8_t **out_ct_arr, uint8_t **out_iv_arr, uint8_t **out_tag_arr) {

    uint32_t *key_arr = (uint32_t *)malloc(batch->reqs_per_suboram * sizeof(uint32_t));
    uint8_t **block_arr = (uint8_t **)malloc(batch->reqs_per_suboram * sizeof(uint8_t *));
    set<uint32_t>client_ids;

    for (int i = 0; i < batch->reqs_per_suboram; i++) {
        block_arr[i] = (uint8_t *)malloc(BLOCK_LEN);
    }
    unsigned char *comm_key = (unsigned char *)"01234567891234567891234567891234";

    int idx = 0;
    for (int i = 0; i < lb->num_suborams; i++) {
        //print_bytes("ct", in_ct_arr[i], get_key_val_buf_sz(batch->reqs_per_suboram));
        //print_bytes("iv", in_iv_arr[i], IV_LEN);
        //print_bytes("tag", in_tag_arr[i], TAG_LEN);
        decrypt_key_val_pairs(comm_key, key_arr, block_arr, batch->reqs_per_suboram, in_ct_arr[i], in_iv_arr[i], in_tag_arr[i], &lb->suboram_replay_ctr_in[i], true);
        //decrypt_key_val_pairs(lb->suboram_comm_key[0], key_arr, block_arr, batch->reqs_per_suboram, in_ct_arr[i], in_iv_arr[i], in_tag_arr[i], &lb->suboram_replay_ctr_in[i], true);
        for (int j = 0; j < batch->reqs_per_suboram; j++) {
            KeyBlockPairBucketItem resp(idx, key_arr[j], block_arr[j]);
            batch->add_suboram_response(resp);
            idx++;
        }
    }
    for (int i = 0; i < batch->reqs_per_suboram; i++) {
        free(block_arr[i]);
    }
    free(key_arr);
    free(block_arr);

    lb->match_responses_to_clients(*batch);

    uint32_t key;
    uint8_t buf[BLOCK_LEN];

    for (int i = 0; i < batch->client_responses.size(); i++) {
        encrypt_key_val(lb->client_comm_key[0], out_ct_arr[i], out_iv_arr[i], out_tag_arr[i], batch->client_responses[i].item.key, batch->client_responses[i].item.block, &lb->client_replay_ctr_out[in_client_id_arr[i]], false);
        client_ids.insert(in_client_id_arr[i]);
        //encrypt_key_val(lb->client_comm_key[in_client_id_arr[i]], out_ct_arr[i], out_iv_arr[i], out_tag_arr[i], batch->client_responses[i].key, batch->client_responses[i].block, &lb->client_replay_ctr_out[in_client_id_arr[i]]);
    }
    set<uint32_t>::iterator it = client_ids.begin();
    while (it != client_ids.end()) {
        lb->client_replay_ctr_out[*it] = lb->client_replay_ctr_out[*it] + 1;
        it++;
    }

    delete batch;
}
