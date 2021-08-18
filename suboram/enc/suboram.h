#ifndef _SUBORAM_H_
#define _SUBORAM_H_
#include <vector>
#include <map>
#include <openssl/evp.h>
#include <mutex>
#include <condition_variable>

#include "../../common/block.h"
#include "../../common/bucket_sort.h"
#include "../../common/ring_buffer.h"

using BucketCTT = BucketCT<block_bucket_item>;

typedef struct {
    uint32_t index;
    uint32_t tag;
    uint32_t tag2;
    bool isRead;
    bool isDummy = false;
    bool isExcess = false;
    uint8_t in_data[BLOCK_LEN];
} req_table_key_metadata;

typedef struct {
    uint32_t orig_pos;
    uint32_t tag;
    bool isDummy;
    bool isExcess;
} bin_assignment;

typedef struct {
    uint32_t index;
    uint8_t out_data[BLOCK_LEN];
} req_table_val;

class SuboramDispatcher {
    private:
        std::vector<block_bucket_item> blocks;
        std::map<uint32_t, std::vector<block_bucket_item *>>block_map;
        EVP_CIPHER_CTX *epoch_key_ctx;

        uint8_t *aes_key;
        uint8_t iv[IV_LEN] = { 0 };

        BucketSortParams params;
        EnclaveBucketSorter<block_bucket_item> *sorter;

        uint8_t *block_key;
        uint8_t block_iv[IV_LEN] = { 0 };
        EVP_CIPHER_CTX *block_key_ctx;
        shared_sort_state<block> *s;
        int blocks_per_bucket;
        int num_buckets;
        BucketCT<block> buf_ct[BLOCK_BUF_BUCKETS];
        Bucket<block> buf[BLOCK_BUF_BUCKETS];
        uint8_t *macs;
        int bucket_idx = 0;

        EVP_CIPHER_CTX *suboram_hash_key;

        int get_pos(uint32_t key, uint32_t *pos);

        int num_threads;

        enum class ThreadFn {
            prf,
            osort_metadata_pass1,
            osort_metadata,
            osort_metadata2_pass1,
            osort_metadata2,
            encrypt,
            decrypt,
            process_blocks,
            process_blocks_h2,
            assign_tags,
            stop,
        };

        struct thread_state {
            ThreadFn fn;
            int n_done;
            int curr_iter = 0;
        };

        std::mutex m;
        std::condition_variable cv;
        thread_state state;
        uint8_t zero_u8 = 0;
        uint32_t batch_sz;
        uint32_t table_len;
        uint32_t *key_arr;
        uint8_t **in_data_arr;
        uint8_t **out_data_arr;
        std::vector<uint8_t> dummy_tags;
        std::vector<req_table_key_metadata> table_metadata;
        std::vector<req_table_val> table_out;
        std::vector<EVP_CIPHER_CTX *> key_ctxs;
        std::vector<EVP_CIPHER_CTX *> idx_key_ctxs;
        std::vector<EVP_CIPHER_CTX *> block_key_ctxs;
        std::vector<uint8_t *> block_ivs;
        std::vector<uint32_t> buf_tags;
        std::vector<uint32_t> buf_tags2;
        std::vector<uint32_t> rnd_idx;
        std::vector<uint32_t> rnd_idx2;
        float avg_bin_size = 2.2377985;
        int num_bins;
        int main_bin_size;
        int nested_table_start;
        int placement_arr_len;
        int combined_table_len;

        int _prf(int thread_id);
        void _decrypt(int thread_id);
        void _encrypt(int thread_id);
        void _process_blocks(int thread_id);
        void _process_blocks_h2(int thread_id);
        int _assign_tags(int thread_id);

        int fetch_from_queue_parallel();
        void notify_threads(ThreadFn fn);
        void wait_for_threads();
    
    public:
        int num_total_blocks;
        int num_local_blocks;
        int num_balancers;
        int num_suborams;
        int suboram_id;

        std::vector<unsigned char *>comm_key;
        std::vector<uint32_t>replay_ctr_in;
        std::vector<uint32_t>replay_ctr_out;

        SuboramDispatcher() : params(0), iv {0} {}
        int set_params(int num_total_blocks, int blocks_per_bucket, int num_balancers, int num_suborams, int suboram_id, int num_threads);
        int init(shared_sort_state<block> *_s);
        int init_bench_sort(shared_sort_state<block_bucket_item> *_s);
        //int init(int num_blocks, int num_balancers, shared_sort_state<block_bucket_item> *s);
        int process_request(uint32_t key, uint8_t *in_data, uint8_t *out_data);
        int gen_request_table(std::vector<req_table_key_bucket_item> &table, std::vector<uint32_t> &idxs, uint32_t *key_arr, uint32_t batch_sz);
        int assign_new_permutation();
        int update_block_map();
        int process_requests_parallel(uint32_t batch_sz, uint32_t num_table_buckets, uint32_t *key_arr, uint8_t **in_data_arr,
                                        uint8_t **out_data_arr);

        void ecall_process_requests_parallel(int *ret, uint8_t *in_ct, uint8_t *in_iv, uint8_t *in_tag, uint8_t *out_ct, uint8_t *out_iv, uint8_t *out_tag, uint32_t batch_sz, int32_t balancer_id, int thread_id);
        int process_requests_thread(int thread_id);

        int scan_blocks();
        int bitonic_sort();
        int parallel_bitonic_sort(int thread_id);
        int parallel_bitonic_sort_nonadaptive(int thread_id);
        int insecure_sort();
        void prefetch_bucket_sort();
        int buffered_bucket_sort(int thread_idx);
        int bucket_sort();
        int melbourne_sort();
        int verify_sorted();

        int fetch_ct_from_queue();
        int fetch_from_queue();
};

bool cmp_block_pos(block_bucket_item a, block_bucket_item b);

bool cmp_req_table_key(req_table_key_bucket_item a, req_table_key_bucket_item b);

#endif
