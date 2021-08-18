#pragma once

#include <openenclave/host.h>
#include "../../common/block.h"
#include "../../common/bucket_sort.h"
#include "../../common/ring_buffer.h"
#include "suboram_u.h"
#include "../enc/suboram.h"

#include "../../common/json.hpp"

using json = nlohmann::json;

class SuboramHost {
private:
    int num_blocks;
    int num_local_blocks;
    int num_balancers;
    int num_suborams;
    int suboram_id;
    int num_buckets;
    int blocks_per_bucket = BLOCKS_PER_BUCKET;
    std::vector<BucketCT<block>> block_ct;
    shared_sort_state<block> s;
    BucketCT<block> buf[BLOCK_BUF_BUCKETS];

    HostBucketSorter<block_bucket_item> sorter;

    SuboramDispatcher mock_suboram;
    int num_threads;
    json oblix_baseline;

    double oblix_access_time = 0;

public:
    oe_enclave_t *enclave;
    int ret;
    std::string protocol;
    std::string sort_type;
    std::string mode;

    SuboramHost(int num_blocks, int num_balancers, int num_suborams, int
                suboram_id, std::string protocol, std::string sort_type, std::string mode,
                int num_threads, json oblix_baseline) : 
                ret(OKAY), num_blocks(num_blocks),
                num_balancers(num_balancers), num_suborams(num_suborams),
                suboram_id(suboram_id), protocol(protocol), sort_type(sort_type), mode(mode),
                num_threads(num_threads), oblix_baseline(oblix_baseline) {
        s.host_q.resize(BLOCK_BUF_BUCKETS*8);
        s.sgx_q.resize(BLOCK_BUF_BUCKETS*8);
    }

    oe_enclave_t* create_enclave(const char* enclave_path);
    void init_enclave();
    void terminate_enclave();

    void mock_init_enclave();

    void fill_block_ct();
    void fetch_from_queue();

    void run_buffered_bucket_sort();
    void run_shuffle();
    void run_process_batch(uint8_t *in_ct, uint8_t *in_iv, uint8_t *in_tag, uint8_t *out_ct, uint8_t *out_iv, uint8_t *out_tag, int batch_sz, uint32_t balancer_id);
    void run_mock_process_batch(uint8_t *in_ct, uint8_t *in_iv, uint8_t *in_tag, uint8_t *out_ct, uint8_t *out_iv, uint8_t *out_tag, int batch_sz, uint32_t balancer_id);
    void run_mock_buffered_bucket_sort();
    void run_parallel_bitonic_sort();
    void run_parallel_bitonic_sort_nonadaptive();

    void bench_sort(int num_trials);
    void bench_scan_blocks(int num_trials);
    void bench_mock_enclave_buffered_bucket_sort(int num_trials);
    void bench_process_batch(int num_trials);

    double get_time_for_b(int b);

    int calc_oblix(int n, int ctr, int b, double access_time);
};

