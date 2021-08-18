#include "suboram.h"

#include <vector>
#include <map>
#include <math.h>
#include <openssl/evp.h>
#include <mutex>
#include <vector>
#include <condition_variable>

#include <openssl/rand.h>

#include "../../common/common.h"
#include "../../common/crypto.h"
#include "../../common/obl_primitives.h"
#include "../../common/par_obl_primitives.h"

#include <chrono>

bool cmp_block_pos(block_bucket_item a, block_bucket_item b) {
    return ObliviousLess(a.item.pos, b.item.pos);
}

bool cmp_req_table_key(req_table_key_bucket_item a, req_table_key_bucket_item b) {
    return ObliviousLess(a.item.tag, b.item.tag);
}

bool cmp_req_table_bin_assignment(req_table_key_metadata a, req_table_key_metadata b) {
    uint64_t cmp_a = (((uint64_t) a.tag) << 32) + a.isDummy;
    uint64_t cmp_b = (((uint64_t) b.tag) << 32) + b.isDummy;
    return ObliviousLess(cmp_a, cmp_b);
}

bool cmp_bin_assignment(bin_assignment a, bin_assignment b) {
    uint64_t cmp_a = (((uint64_t) a.tag) << 32) + a.isDummy;
    uint64_t cmp_b = (((uint64_t) b.tag) << 32) + b.isDummy;
    return ObliviousLess(cmp_a, cmp_b);
}

bool cmp_bin_assignment_orig_pos(bin_assignment a, bin_assignment b) {
    return ObliviousLess(a.orig_pos, b.orig_pos);
}

uint64_t get_cmp(req_table_key_metadata t) {
    uint64_t ret = 0;
    ret += ((uint64_t) t.isExcess) << 62;
    ret += ((uint64_t) t.tag) << 1;
    ret += t.isDummy;
    return ret;
}

bool cmp_req_table_key_metadata(req_table_key_metadata a, req_table_key_metadata b) {
    uint64_t cmp_a = 0;
    uint64_t cmp_b = 0;
    // sort by excess first
    cmp_a += ((uint64_t) a.isExcess) << 62;
    cmp_b += ((uint64_t) b.isExcess) << 62;
    // then sort by bin
    cmp_a += ((uint64_t) a.tag) << 1;
    cmp_b += ((uint64_t) b.tag) << 1;
    // then by dummy
    cmp_a += a.isDummy;
    cmp_b += b.isDummy;
    return ObliviousLess<uint64_t>(cmp_a, cmp_b);
}

int SuboramDispatcher::set_params(int num_total_blocks, int blocks_per_bucket, int num_balancers, int num_suborams, int suboram_id, int num_threads) {
    int rv = ERROR;
    this->blocks_per_bucket = blocks_per_bucket;
    this->num_total_blocks = num_total_blocks;
    this->num_balancers = num_balancers;
    this->num_suborams = num_suborams;
    this->suboram_id = suboram_id;
    printf("Num total blocks: %d\n", num_total_blocks);

    block_key = (uint8_t *) "0123456789123456"; // TODO: Change keys

    uint8_t *thread_iv;

    for (int i = 0 ; i < num_threads; i++) {
        block_key_ctx = EVP_CIPHER_CTX_new();
        EVP_EncryptInit_ex(block_key_ctx, EVP_aes_256_gcm(), NULL, block_key, NULL);
        block_key_ctxs.push_back(block_key_ctx);
        thread_iv = (uint8_t *) malloc(sizeof(uint8_t)*IV_LEN);
        memcpy(thread_iv, block_iv, IV_LEN);
        block_ivs.push_back(thread_iv);
    }

    block_key_ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(block_key_ctx, EVP_aes_256_gcm(), NULL, block_key, NULL);

    suboram_hash_key = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(suboram_hash_key, EVP_aes_128_ecb(), NULL, block_key, NULL);

    num_local_blocks = get_blocks_per_suboram(suboram_hash_key, num_suborams, num_total_blocks);
    printf("Num local blocks = %d\n", num_local_blocks);
    num_buckets = num_local_blocks / blocks_per_bucket;

    macs = (uint8_t *) malloc(num_buckets*TAG_LEN);

    for (int b = 0; b < BLOCK_BUF_BUCKETS; b++) {
        buf[b].init(blocks_per_bucket);
        buf_ct[b].init(blocks_per_bucket);
    }

    comm_key.push_back((unsigned char *)"01234567891234567891234567891234");
    for (int i = 0; i < num_balancers; i++) {
        replay_ctr_in.push_back(0);
        replay_ctr_out.push_back(1);
    }
    this->num_threads = num_threads;

    return OKAY;
}

int SuboramDispatcher::init(shared_sort_state<block> *s) {
    int rv = ERROR;
    this->s = s;
    printf("dispatcher init\n");
    if (s == NULL) return OKAY;

    std::vector<int> indexes;
    for (int i = 0; i < num_total_blocks; i++) {
        if ((get_suboram_for_req(suboram_hash_key, i, num_suborams) == suboram_id) & (indexes.size() < num_local_blocks)) {
            indexes.push_back(i);
        }
    }

    int _idx = 0;
    for (int b = 0; b < num_buckets; b += BLOCK_BUF_BUCKETS) {
        fetch_ct_from_queue();
        for (int bucket = 0; bucket < BLOCK_BUF_BUCKETS; bucket++) {
            for (int i = 0; i < blocks_per_bucket; i++) {
                if (_idx < indexes.size()) {
                    //printf("Set %d -> %d\n", _idx, indexes[_idx]);
                    buf[bucket].items[i].pos = indexes[_idx];
                } else {
                    //printf("Filled %d with -1\n", _idx);
                    buf[bucket].items[i].pos = -1;   // extra slot
                }
                _idx++;
            }
            buf[bucket].encrypt(block_key_ctx, block_iv, buf_ct[bucket]);
            memcpy(&macs[(b+bucket)*TAG_LEN], buf_ct[bucket].tag, TAG_LEN);
        }
        s->sgx_q.write(buf_ct, BLOCK_BUF_BUCKETS);
    }

    cleanup:
    return rv;
}

// Just for benchmarking sorts
int SuboramDispatcher::init_bench_sort(shared_sort_state<block_bucket_item> *s) {
    printf("init bench sort\n");
    if (s == NULL) return OKAY;
    int rv = ERROR;
    blocks = std::vector<block_bucket_item>(num_local_blocks);
    int idx = 0;
    for (int i = 0; i < num_total_blocks; i++) {
        if ((get_suboram_for_req(suboram_hash_key, i, num_suborams) == suboram_id) & (idx < num_local_blocks)) {
            blocks[idx].index = idx;
            blocks[idx].item.pos = num_local_blocks - idx;
            idx++;
        }
    }
    params = BucketSortParams(num_local_blocks);

    aes_key = (uint8_t *) "0123456789123456"; // TODO: Change keys
    CHECK_A(sorter = new EnclaveBucketSorter<block_bucket_item>(s, aes_key, iv, num_threads));
    sorter->init_bucket_sort();
    cleanup:
    return rv;
}

int SuboramDispatcher::fetch_from_queue() {
    fetch_ct_from_queue();
    for (int i = 0; i < BLOCK_BUF_BUCKETS; i++) {
        uint8_t* expected_mac = &macs[bucket_idx*TAG_LEN];
        for (int t = 0; t < TAG_LEN; t++) {
            if (expected_mac[t] != buf_ct[i].tag[t]) {
                printf("TAG DOES NOT MATCH:\n\texpected 0x");
                for (int t = 0; t < TAG_LEN; t++) {
                    printf(" %02x ", expected_mac[t]);
                }
                printf("\n\tbut got  0x");
                for (int t = 0; t < TAG_LEN; t++) {
                    printf(" %02x ", buf_ct[i].tag[t]);
                }
                printf("\n");
                break;
            }
        }
        buf_ct[i].decrypt(block_key_ctx, &buf[i].items[0]);
        bucket_idx = (bucket_idx + 1) % (num_buckets);
    }
    return OKAY;
}
int SuboramDispatcher::fetch_from_queue_parallel() {
    fetch_ct_from_queue();
    notify_threads(ThreadFn::decrypt);
    _decrypt(0);
    wait_for_threads();
    bucket_idx = (bucket_idx + BLOCK_BUF_BUCKETS) % num_buckets;
    return OKAY;
}

void SuboramDispatcher::_decrypt(int thread_id) {
    auto bounds = get_cutoffs_for_thread(thread_id, BLOCK_BUF_BUCKETS, num_threads);
    int local_bucket_idx = (bucket_idx + bounds.first) % num_buckets;
    EVP_CIPHER_CTX *block_key_ctx = block_key_ctxs[thread_id];
    for (int i = bounds.first; i < bounds.second; i++) {
        uint8_t* expected_mac = &macs[local_bucket_idx*TAG_LEN];
        for (int t = 0; t < TAG_LEN; t++) {
            if (expected_mac[t] != buf_ct[i].tag[t]) {
                printf("TAG DOES NOT MATCH:\n\texpected 0x");
                for (int t = 0; t < TAG_LEN; t++) {
                    printf(" %02x ", expected_mac[t]);
                }
                printf("\n\tbut got  0x");
                for (int t = 0; t < TAG_LEN; t++) {
                    printf(" %02x ", buf_ct[i].tag[t]);
                }
                printf("\n");
                break;
            }
        }
        buf_ct[i].decrypt(block_key_ctx, &buf[i].items[0]);
        local_bucket_idx = (local_bucket_idx + 1) % (num_buckets);
    }
}

int SuboramDispatcher::fetch_ct_from_queue() {
    volatile int read = 0;
    int i = 0;
    while (read <= 0) {
        read = s->host_q.read_full(buf_ct, BLOCK_BUF_BUCKETS);
        i++;
    }
    return OKAY;
}

int SuboramDispatcher::scan_blocks() {
    int total = 0;
    for (int b = 0; b < num_buckets; b += BLOCK_BUF_BUCKETS) {
        fetch_from_queue();
        for (int bucket = 0; bucket < BLOCK_BUF_BUCKETS; bucket++) {
            for (int i = 0; i < blocks_per_bucket; i++) {
                total += buf[bucket].items[i].pos;
            }
            buf[bucket].encrypt(block_key_ctx, block_iv, buf_ct[bucket]);
            memcpy(&macs[(b+bucket)*TAG_LEN], buf_ct[bucket].tag, TAG_LEN);
        }
        s->sgx_q.write(buf_ct, BLOCK_BUF_BUCKETS);
    }
    return OKAY;
}

/*
int SuboramDispatcher::init(int num_blocks, int num_balancers, shared_sort_state<block_bucket_item> *s) {
    int rv = ERROR;

    aes_key = (uint8_t *) "0123456789123456"; // TODO: Change keys

    EVP_CIPHER_CTX *suboram_hash_key = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(suboram_hash_key, EVP_aes_128_ecb(), NULL, aes_key, NULL);

    int num_local_blocks = get_blocks_per_suboram(suboram_hash_key, num_suborams, num_total_blocks);

    blocks = std::vector<block_bucket_item>(num_local_blocks);
    int idx = 0;
    for (int i = 0; i < num_total_blocks; i++) {
        if ((get_suboram_for_req(suboram_hash_key, i, num_suborams) == suboram_id) & (idx < num_local_blocks)) {
            blocks[idx].index = i;
            blocks[idx].item.pos = idx;
            idx++;
        }
    }
    epoch_key_ctx = EVP_CIPHER_CTX_new();

    comm_key.push_back((unsigned char *)"01234567891234567891234567891234");
    for (int i = 0; i < num_balancers; i++) {
        replay_ctr_in.push_back(0);
        replay_ctr_out.push_back(1);
    }

    params = BucketSortParams(num_local_blocks);

    CHECK_A(sorter = new EnclaveBucketSorter<block_bucket_item>(s, aes_key, iv));
    sorter->init_bucket_sort();

    cleanup:
    if (suboram_hash_key) free(suboram_hash_key);
    return rv;
}

int SuboramDispatcher::scan_blocks() {
    int total = 0;
    int i = 0;
    for (auto &b : blocks) {
        // if (i % 24 == 0) {
        //     __builtin_prefetch(&blocks[i+24]);
        // }
        total += b.index;
        i += 1;
    }
    return total;
}
*/

bool _insecure_cmp_block(block_bucket_item a, block_bucket_item b) {
    return a.item.pos < b.item.pos;
    //return a.index < b.index;
}

int SuboramDispatcher::get_pos(uint32_t key, uint32_t *pos) {
    int rv;

    CHECK_C (prf(epoch_key_ctx, (uint8_t *)pos, sizeof(uint32_t), (uint8_t *)&key, sizeof(uint32_t)));

cleanup:
    return rv;
}

int SuboramDispatcher::process_request(uint32_t key, uint8_t *in_data, uint8_t *out_data) {
    uint32_t pos;
    int rv;
    std::vector<block_bucket_item *> block_list;
    block_bucket_item *b;
    uint8_t zero_u8 = 0;
    bool isRead;
    bool isMatch;

    CHECK_C (get_pos(key, &pos));
    block_list = block_map[pos];

    for (int i = 0; i < block_list.size(); i++) {
        isRead = true;
        b = block_list[i];
        isMatch = ObliviousEqual(b->index, key);
        /* Read if in_data all 0s. Write contents of in_data if not all 0s. */
        for (int j = 0; j < BLOCK_LEN; j++) {
            isRead &= ObliviousEqual(in_data[j], zero_u8);
        }
        for (int j = 0; j < BLOCK_LEN; j++) {
            b->item.bytes[j] = ObliviousChoose(!isRead && isMatch, in_data[j], b->item.bytes[j]);
            out_data[j] = ObliviousChoose(isMatch, b->item.bytes[j], out_data[j]);
        }
    }
cleanup:
    return rv;
}

int SuboramDispatcher::assign_new_permutation() {
    int rv = OKAY;
    uint8_t key_bytes[32];

    CHECK_C (RAND_bytes(key_bytes, 32));
    CHECK_C (EVP_EncryptInit_ex(epoch_key_ctx, EVP_aes_128_ecb(), NULL, key_bytes, NULL));

    for (int i = 0; i < blocks.size(); i++) {
        CHECK_C (get_pos(blocks[i].index, &blocks[i].item.pos));
    }

cleanup:
    return rv;
}

int SuboramDispatcher::update_block_map() {
    block_map.clear();
    for (int i = 0; i < blocks.size(); i++) {
        block_map[blocks[i].item.pos].push_back(&blocks[i]);
    }
    return OKAY;
}

int SuboramDispatcher::bitonic_sort() {
    //ObliviousSort(blocks.begin(), blocks.end(), cmp_bucket_item_index<block_bucket_item>);
    ObliviousSort(blocks.begin(), blocks.end(), cmp_block_pos);
    return OKAY;
}

int SuboramDispatcher::parallel_bitonic_sort(int thread_id) {
    if (num_threads == 1) {
        ObliviousSort(blocks.begin(), blocks.end(), cmp_block_pos);
    } else {
        ObliviousSortParallel(blocks.begin(), blocks.end(), cmp_block_pos, num_threads, thread_id);
    }
    return OKAY;
}

int SuboramDispatcher::parallel_bitonic_sort_nonadaptive(int thread_id) {
    if (num_threads == 1) {
        ObliviousSort(blocks.begin(), blocks.end(), cmp_block_pos);
    } else {
        ObliviousSortParallelNonAdaptive(blocks.begin(), blocks.end(), cmp_block_pos, num_threads, thread_id);
    }
    return OKAY;
}


int SuboramDispatcher::insecure_sort() {
    std::sort(blocks.begin(), blocks.end(), _insecure_cmp_block);
    return OKAY;
}

bool _cmp_block_melbourne(block_bucket_item a, block_bucket_item b) {
/*
    uint64_t a_64 = ((uint64_t)a.bucket << 32) | a.pos;
    uint64_t b_64 = ((uint64_t)b.bucket << 32) | b.pos;
    return ObliviousLess(a_64, b_64);
*/
}

int SuboramDispatcher::melbourne_sort() {
/*
    int rv = ERROR;
    uint32_t dummy_pos = blocks.size() + 1;
    int sqrt_n = ceil(sqrt(blocks.size()));
    int log_n = ceil(log(blocks.size()));
    int min_entries = ceil(2.718*log_n);
    int bucket_size = min_entries * sqrt_n;
    int bucket_size_padded = bucket_size + sqrt_n;
    block empty_block;
    std::vector<block> bucket_padded(bucket_size_padded, empty_block);
    std::vector<block> buckets(sqrt_n * bucket_size);

    // Remap block indexes to random values
    int log2_n = ceil(log2(blocks.size()));
    int log2_n_bytes = ceil(log2_n / 8.0);
    unsigned char *key;
    uint8_t *tmp;
    CHECK_A (key = (unsigned char *) malloc(16 * sizeof(unsigned char)));
    CHECK_A (tmp = (uint8_t *) malloc(log2_n_bytes * sizeof(uint8_t)));
    {
    RAND_bytes(key, 16);
    EVP_CIPHER_CTX *prf_ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(prf_ctx, EVP_aes_128_ecb(), NULL, key, NULL);
    for (auto &b : blocks) {
        prf(prf_ctx, tmp, log2_n_bytes, (uint8_t *)&b.index, sizeof(b.index));
        b.pos = 0;
        int last = log2_n_bytes-1;
        for (int i = 0; i < last; i++) {
            b.pos += tmp[i] << (8 * i);
        }
        int to_truncate = log2_n_bytes * 8 - log2_n;
        tmp[last] = tmp[last] << to_truncate;
        tmp[last] = tmp[last] >> to_truncate;
        b.pos += tmp[last] << (log2_n_bytes-1)*8;
        b.bucket = floor(b.pos / sqrt_n);
    }
    }
    uint8_t *tags;
    CHECK_A (tags = (uint8_t *) malloc(bucket_size_padded*sizeof(uint8_t)));
    {
    int len;
    for (int i = 0; i < sqrt_n; i++) {
        for (int j = 0; j < sqrt_n; j++) {
            for (int k = 0; k < min_entries; k++) {
                bucket_padded[j*min_entries+k].index = dummy_pos;
                bucket_padded[j*min_entries+k].bucket = j;
                bucket_padded[j*min_entries+k].pos= dummy_pos;
            }
        }
        len = sqrt_n;
        if (i == sqrt_n - 1 && blocks.size() % sqrt_n != 0) {
            len = blocks.size() % sqrt_n;
        }
        memcpy(&bucket_padded[bucket_size], &blocks[sqrt_n*i], len*sizeof(block));
        for (int j = len; j < sqrt_n; j++) {
            bucket_padded[bucket_size+j].index = dummy_pos;
            bucket_padded[bucket_size+j].bucket = j;
            bucket_padded[bucket_size+j].pos = dummy_pos;
        }
        ObliviousSort(bucket_padded.begin(), bucket_padded.end(), _cmp_block_melbourne);
        int ctr = 0;
        uint32_t prev = sqrt_n + 1;
        int j = 0;
        for (auto &b : bucket_padded) {
            ctr = ObliviousChoose(ObliviousEqual(prev, b.bucket), ctr+1, 0);
            tags[j] = ObliviousLess(ctr, min_entries);
            prev = b.bucket;
            j++;
        }
        ObliviousCompact(bucket_padded.begin(), bucket_padded.end(), tags);
        for (auto &b : bucket_padded)
        for (int j = 0; j < sqrt_n; j++) {
            memcpy(&buckets[j*bucket_size + i*min_entries], &bucket_padded[j*min_entries], min_entries*sizeof(block));
        }
    }
    // TODO: Check if this is still oblivious (cite BucketSort)...
        int total = 0;
        for (int i = 0; i < sqrt_n; i++)
        {
            int cnt = 0;
            for (int j = 0; j < bucket_size; j++)
            {
                bool not_dummy = !ObliviousEqual(buckets[i * bucket_size + j].pos, dummy_pos);
                tags[j] = not_dummy;
                cnt += not_dummy;
            }
            ObliviousCompact(buckets.begin() + i * bucket_size, buckets.begin() + i * bucket_size + bucket_size, tags);
            memcpy(&blocks[total], &buckets[i * bucket_size], cnt * sizeof(block));
            ObliviousSort(blocks.begin() + total, blocks.begin() + total + cnt, cmp_block_pos);
            total += cnt;
        }
    }
    std::sort(blocks.begin(), blocks.end(), _insecure_cmp_block);

    cleanup:
    return OKAY;
*/
}

std::condition_variable cv;
std::mutex lock;
int c_idx, s_idx;
bool stop, lhs;
std::vector<block_bucket_item> curr_level, next_level;
int half_stride;

void SuboramDispatcher::prefetch_bucket_sort() {
    int next_c_idx, next_s_idx;
    while (true) {
        std::unique_lock<std::mutex> lk(lock);
        cv.wait(lk);
        if (stop) {
            lk.unlock();
            break;
        }

        block_bucket_item *cl = curr_level.data();
        block_bucket_item *nl = next_level.data();
        if (lhs) {
            next_c_idx = c_idx + half_stride*2*params.z;
            for (int i = 0; i < half_stride * 2 * params.z; i++)
            {
                *((int volatile *)&cl[next_c_idx + i].index) = cl[next_c_idx + i].index;
            }
        } else {
            next_c_idx = 0;
            for (int i = 0; i < half_stride * 2 * params.z; i++)
            {
                *((int volatile *)&nl[next_c_idx + i].index) = nl[next_c_idx + i].index;
            }
        }


        /*
        block *nl = next_level.data();
        if (lhs) {
            // Read in the right hand side
            for (int k = 0; k < half_stride; k++)
            {
                next_s_idx = s_idx + z;
                for (int i = 0; i < z; i++) {
                    *((int volatile *) &nl[next_s_idx + i].index) = nl[next_s_idx + i].index;
                }
                next_s_idx += 2*z;
                for (int i = 0; i < z; i++) {
                    *((int volatile *) &nl[next_s_idx + i].index) = nl[next_s_idx + i].index;
                }
                next_s_idx += z;
            }
        } else {
            next_s_idx = s_idx + half_stride*2*z;
            for (int k = 0; k < half_stride; k++)
            {
                for (int i = 0; i < z; i++) {
                    *((int volatile *) &nl[next_s_idx + i].index) = nl[next_s_idx + i].index;
                }
                next_s_idx += 2*z;
                for (int i = 0; i < z; i++) {
                    *((int volatile *) &nl[next_s_idx + i].index) = nl[next_s_idx + i].index;
                }
                next_s_idx += 2*z;
            }
        }
        */

        /*
        block *cl = curr_level.data();
        next_c_idx = c_idx + 2*z;
        for (int i = 0; i < 2*z; i++) {
            *((int volatile *) &cl[next_c_idx + i].index) = cl[next_c_idx + i].index;
        }
        block *nl = next_level.data();
        next_s_idx = s_idx + 4*z;
        if (!lhs) {
            next_s_idx += z;
        }
        for (int i = 0; i < z; i++) {
            *((int volatile *) &nl[next_s_idx + i].index) = nl[next_s_idx + i].index;
        }
        next_s_idx += 2*z;
        for (int i = 0; i < z; i++) {
            *((int volatile *) &nl[next_s_idx + i].index) = nl[next_s_idx + i].index;
        }
        */
        //printf("c_idx: %d, s_idx: %d\n", c_idx, s_idx);
        lk.unlock();
    }
}

void assign_random_pos(EVP_CIPHER_CTX *prf_ctx, int num_bytes, int num_bits, block_bucket_item *b, uint8_t *tmp) {
    prf(prf_ctx, tmp, num_bytes, (uint8_t *)&b->index, sizeof(b->index));
    b->bucket_sort_pos = 0;
    int last = num_bytes-1;
    for (int i = 0; i < last; i++) {
        b->bucket_sort_pos += tmp[i] << (8 * i);
    }
    int to_truncate = num_bytes * 8 - num_bits;
    tmp[last] = tmp[last] << to_truncate;
    tmp[last] = tmp[last] >> to_truncate;
    b->bucket_sort_pos += tmp[last] << (num_bytes-1)*8;
}

void merge_split(block_bucket_item *input0, block_bucket_item *input1, std::vector<block_bucket_item> &input,
            uint8_t *tags, int z, int i, int idx, int bitlen) {
    uint32_t mask_dummy = 1 << bitlen;
    uint32_t mask1 = 1 << i;
    int count0 = 0;
    int count1 = 0;
    // Count how many real values will end up in output1 and output2
    bool cond;
    bool is_dummy_cond;
    for(int i = 0; i < z; i++) {
        cond = input0[i].bucket_sort_pos & mask1;
        is_dummy_cond = input0[i].bucket_sort_pos & mask_dummy;
        count1 += cond;
        count0 += 1-cond-is_dummy_cond;
    }
    for(int i = 0; i < z; i++) {
        cond = input1[i].bucket_sort_pos & mask1;
        is_dummy_cond = input1[i].bucket_sort_pos & mask_dummy;
        count1 += cond;
        count0 += 1-cond-is_dummy_cond;
    }
    // Retag dummies and values
    int total_dummies0 = z - count0;
    int dummies0 = 0;
    for(int i = 0; i < z; i++) {
        if (input0[i].bucket_sort_pos & mask_dummy) {
            if (dummies0 < total_dummies0) {
                tags[i] = 1;
                dummies0++;
            } else {
                tags[i] = 0;
            }
        } else {
            tags[i] = (bool) (input0[i].bucket_sort_pos & mask1);
            tags[i] = 1 - tags[i];
        }
    }
    for(int i = 0; i < z; i++) {
        if (input1[i].bucket_sort_pos & mask_dummy) {
            if (dummies0 < total_dummies0) {
                tags[i+z] = 1;
                dummies0++;
            } else {
                tags[i+z] = 0;
            }
        } else {
            tags[i+z] = (bool) (input1[i].bucket_sort_pos & mask1);
            tags[i+z] = 1 - tags[i+z];
        }
    }
    //std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    ObliviousCompact(input.begin() + idx, input.begin() + idx + 2*z, tags);
    //std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    //printf("OCompact for %d blocks: %f us\n", 2*z, ((double)std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()));
}

int SuboramDispatcher::buffered_bucket_sort(int thread_idx) {
    return sorter->sort(blocks, cmp_block_pos, thread_idx);
}

int SuboramDispatcher::bucket_sort() {
    int rv = ERROR;
    stop = false;
    int log_total_buckets_bytes = ceil((float) params.log_total_buckets / 8);
    int real_z = params.num_blocks / params.total_buckets;
    unsigned char *key;
    //printf("levels: %d, z: %d, blocksize: %u\n", params.log_total_buckets, params.z, sizeof(block));
    uint8_t *tmp;
    CHECK_A (key = (unsigned char *) malloc(16 * sizeof(unsigned char)));
    CHECK_A (tmp = (uint8_t *) malloc(log_total_buckets_bytes * sizeof(uint8_t)));
    {
    RAND_bytes(key, 16);
    EVP_CIPHER_CTX *prf_ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(prf_ctx, EVP_aes_128_ecb(), NULL, key, NULL);
    for (auto &b : blocks) {
        assign_random_pos(prf_ctx, log_total_buckets_bytes, params.log_total_buckets, &b, tmp);
    }
    block_bucket_item empty_block;
    curr_level = std::vector<block_bucket_item>(params.total_buckets * params.z);
    for (uint32_t b = 0; b < params.total_buckets; b++) {
        for (int i = 0; i < real_z; i++) {
            curr_level[b*params.z + i] = blocks[b*real_z + i];
        }
        for (int i = real_z; i < params.z; i++) {
            curr_level[b*params.z + i].item.pos = params.total_buckets;
        }
    }

    uint8_t *tags;
    CHECK_A (tags = (uint8_t *) malloc(2*params.z*sizeof(uint8_t)));
    {

    /* Uncomment for a different locality approach. Slightly worse than current one.
    std::vector<block> lhs = std::vector<block>(z*total_buckets/2, empty_block);
    std::vector<block> lhs_res = std::vector<block>(z*total_buckets/2, empty_block);
    std::vector<block> rhs = std::vector<block>(z*total_buckets/2, empty_block);
    std::vector<block> rhs_res = std::vector<block>(z*total_buckets/2, empty_block);

    for (int j = 0; j < total_buckets / 2; j++) {
        for (int i = 0; i < real_z; i++) {
            lhs[j*z + i] = blocks[2*j*real_z + i];
        }
        for (int i = real_z; i < z; i++) {
            lhs[j*z + i].pos = total_buckets;
        }
        for (int i = 0; i < real_z; i++) {
            rhs[j*z + i] = blocks[2*j*real_z + z + i];
        }
        for (int i = real_z; i < z; i++) {
            rhs[j*z + i].pos = total_buckets;
        }
    }

    for (int i = 0; i < log_total_buckets-1; i++) {
        int stride = 1 << (i + 1);
        int half_stride = 1 << i;
        int operand_idx = 0;
        int res_idx = 0;
        for (int s = 0; s < total_buckets / stride / 2; s++) {
            res_idx = s * half_stride * z;
            for (int k = 0; k < half_stride; k++) {
                memcpy(&lhs_res[res_idx], &lhs[operand_idx], sizeof(block)*z);
                memcpy(&lhs_res[res_idx+z], &rhs[operand_idx], sizeof(block)*z);
                merge_split(&lhs_res[res_idx], &lhs_res[res_idx+z], lhs_res, tags, z, i, res_idx, log_total_buckets);
                operand_idx += z;
                res_idx += 2*z;
            }
            res_idx = s * half_stride * z;
            for (int k = 0; k < half_stride; k++) {
                memcpy(&rhs_res[res_idx], &lhs[operand_idx], sizeof(block)*z);
                memcpy(&rhs_res[res_idx+z], &rhs[operand_idx], sizeof(block)*z);
                merge_split(&rhs_res[res_idx], &rhs_res[res_idx+z], rhs_res, tags, z, i, res_idx, log_total_buckets);
                operand_idx += z;
                res_idx += 2*z;
            }
        }
        lhs.swap(lhs_res);
        rhs.swap(rhs_res);
    }

    std::vector<block>().swap(lhs_res);
    std::vector<block>().swap(rhs_res);
    std::vector<block> curr_level = std::vector<block>(total_buckets * z, empty_block);
    for (int j = 0; j < total_buckets / 2; j++) {
        memcpy(&curr_level[2*j*z], &lhs[j*z], sizeof(block)*z);
        memcpy(&curr_level[2*j*z+z], &rhs[j*z], sizeof(block)*z);
        merge_split(&curr_level[2*j*z], &curr_level[2*j*z+z], curr_level, tags, z, log_total_buckets, 2*j*z, log_total_buckets);
    }
    std::vector<block>().swap(lhs);
    std::vector<block>().swap(rhs);
    */

    next_level = std::vector<block_bucket_item>(params.z*params.total_buckets);
    c_idx = 0;
    s_idx = 0;
    for (int i = 0; i < params.log_total_buckets - 1; i++) {
        int stride = 1 << (i + 1);
        half_stride = 1 << i;
        for (int s = 0; s < params.total_buckets / stride / 2; s++) {
            {
                std::lock_guard<std::mutex> lk(lock);
                s_idx = s*stride*params.z;
                lhs = true;
            }
            cv.notify_one();
            for (int k = 0; k < half_stride; k++) {
                //__builtin_prefetch(&next_level[s_idx], 0, 1);
                //__builtin_prefetch(&next_level[s_idx+2*z], 0, 1);
                merge_split(&curr_level[c_idx], &curr_level[c_idx+params.z], curr_level, tags, params.z, i, c_idx, params.log_total_buckets);
                memcpy(&next_level[s_idx], &curr_level[c_idx], sizeof(block_bucket_item)*params.z);
                memcpy(&next_level[s_idx+2*params.z], &curr_level[c_idx+params.z], sizeof(block_bucket_item)*params.z);
                {
                    std::lock_guard<std::mutex> lk(lock);
                    c_idx += 2 * params.z;
                    s_idx += 4 * params.z;
                }
            }
            {
                std::lock_guard<std::mutex> lk(lock);
                s_idx = s*stride*params.z;
                lhs = false;
            }
            cv.notify_one();
            for (int k = 0; k < half_stride; k++) {
                //__builtin_prefetch(&next_level[s_idx+z], 0, 1);
                //__builtin_prefetch(&next_level[s_idx+3*z], 0, 1);
                merge_split(&curr_level[c_idx], &curr_level[c_idx+params.z], curr_level, tags, params.z, i, c_idx, params.log_total_buckets);
                memcpy(&next_level[s_idx+params.z], &curr_level[c_idx], sizeof(block_bucket_item)*params.z);
                memcpy(&next_level[s_idx+3*params.z], &curr_level[c_idx+params.z], sizeof(block_bucket_item)*params.z);
                {
                    std::lock_guard<std::mutex> lk(lock);
                    c_idx += 2 * params.z;
                    s_idx += 4 * params.z;
                }
            }
        }
        {
            std::lock_guard<std::mutex> lk(lock);
            c_idx = 0;
        }
        next_level.swap(curr_level);
    }
    {
        std::lock_guard<std::mutex> lk(lock);
        stop = true;
    }
    cv.notify_one();

    int block_idx = 0;
    int bucket_end = 0;
    for (int j = 0; j < params.total_buckets / 2; j++) {
        int idx = 2*j*params.z;
        merge_split(&curr_level[2*j*params.z], &curr_level[2*j*params.z+params.z], curr_level, tags, params.z, params.log_total_buckets, 2*j*params.z, params.log_total_buckets);

        for (int i = 0; i < 2*params.z; i++) {
            tags[i] = (bool) (curr_level[idx+i].item.pos & params.total_buckets);
            tags[i] = 1 - tags[i];
        }
        ObliviousCompact(curr_level.begin() + idx, curr_level.begin()+idx+params.z, tags);
        ObliviousCompact(curr_level.begin()+idx+params.z, curr_level.begin()+idx+params.z+params.z, tags);
        bucket_end = 0;
        for (int i = 0; i < params.z; i++) {
            if(curr_level[idx+i].bucket_sort_pos >= params.total_buckets) {
                bucket_end = i;
                break;
            }
            assign_random_pos(prf_ctx, log_total_buckets_bytes, params.log_num_blocks, &curr_level[idx+i], tmp);
        }
        memcpy(&blocks[block_idx], &curr_level[idx], sizeof(block_bucket_item) * bucket_end);
        ObliviousSort(blocks.begin() + block_idx, blocks.begin() + block_idx + bucket_end, cmp_block_pos);
        block_idx += bucket_end;
        bucket_end = 0;
        for (int i = params.z; i < 2*params.z; i++) {
            if(curr_level[idx+i].bucket_sort_pos >= params.total_buckets) {
                bucket_end = i;
                break;
            }
            assign_random_pos(prf_ctx, log_total_buckets_bytes, params.log_num_blocks, &curr_level[idx+i], tmp);

        }
        memcpy(&blocks[block_idx], &curr_level[idx+params.z], sizeof(block_bucket_item) * bucket_end);
        ObliviousSort(blocks.begin() + block_idx, blocks.begin() + block_idx + bucket_end, cmp_block_pos);
        block_idx += bucket_end;
    }
    std::sort(blocks.begin(), blocks.end(), _insecure_cmp_block);
    }
    }

    cleanup:
    return OKAY;
}

int SuboramDispatcher::verify_sorted() {
    bool sorted = true;
    for (int i = 0; i < blocks.size() - 1; i++) {
        if (blocks[i].item.pos > blocks[i+1].item.pos) {
            sorted = false;
        }
    }
    return (int) sorted;
}

int SuboramDispatcher::process_requests_parallel(uint32_t batch_sz, uint32_t num_table_buckets, uint32_t *key_arr, uint8_t **in_data_arr,
                                                 uint8_t **out_data_arr) {
    int rv = ERROR;
    // Reset state
    state.curr_iter = 0;
    this->batch_sz = batch_sz;
    table_len = 0;
    this->key_arr = key_arr;
    this->in_data_arr = in_data_arr;
    this->out_data_arr = out_data_arr;
    key_ctxs.clear();
    idx_key_ctxs.clear();

    num_bins = ceil(batch_sz / avg_bin_size);
    main_bin_size = ceil(5*avg_bin_size);
    combined_table_len = main_bin_size*num_bins*2;
    nested_table_start = main_bin_size*num_bins;
    table_metadata = std::vector<req_table_key_metadata>(combined_table_len+batch_sz);
    placement_arr_len = main_bin_size*num_bins + batch_sz;
    table_out = std::vector<req_table_val>(combined_table_len);

    uint8_t key_bytes[32];
    uint8_t idx_key_bytes[32];
    EVP_CIPHER_CTX *key_ctx = NULL;
    EVP_CIPHER_CTX *idx_key_ctx = NULL;
    buf_tags = std::vector<uint32_t>(blocks_per_bucket*BLOCK_BUF_BUCKETS);
    buf_tags2 = std::vector<uint32_t>(blocks_per_bucket*BLOCK_BUF_BUCKETS);
    rnd_idx = std::vector<uint32_t>(blocks_per_bucket*BLOCK_BUF_BUCKETS);
    rnd_idx2 = std::vector<uint32_t>(blocks_per_bucket*BLOCK_BUF_BUCKETS);
    dummy_tags = std::vector<uint8_t>(combined_table_len);
    uint32_t last_tag = 0;

    CHECK_C (RAND_bytes(key_bytes, 32));
    CHECK_C (RAND_bytes(idx_key_bytes, 32));
    for (int i = 0; i < num_threads; i++) {
        CHECK_A (key_ctx = EVP_CIPHER_CTX_new());
        CHECK_C (EVP_EncryptInit_ex(key_ctx, EVP_aes_128_ecb(), NULL, key_bytes, NULL));
        key_ctxs.push_back(key_ctx);
        CHECK_A (idx_key_ctx = EVP_CIPHER_CTX_new());
        CHECK_C (EVP_EncryptInit_ex(idx_key_ctx, EVP_aes_128_ecb(), NULL, idx_key_bytes, NULL));
        idx_key_ctxs.push_back(idx_key_ctx);
    }
    notify_threads(ThreadFn::prf);
    _prf(0);
    wait_for_threads();

    for (int i = 0; i < num_bins; i++) {
        for (int j = 0; j < main_bin_size; j++) {
            int idx = batch_sz+i*main_bin_size+j;
            table_metadata[idx].tag = i;
            table_metadata[idx].tag2 = i;
            table_metadata[idx].isDummy = true;
        }
    }

    notify_threads(ThreadFn::osort_metadata_pass1);
    ObliviousSortParallel(table_metadata.begin(), table_metadata.begin()+placement_arr_len, cmp_req_table_bin_assignment, num_threads, 0);
    wait_for_threads();
    {
        int curr_bucket_size = 0;
        int curr_bucket_size_inc;
        uint32_t curr_bucket = 0;
        bool new_bucket;
        for (int i = 0; i < placement_arr_len; i++) {
            new_bucket = !ObliviousEqual(table_metadata[i].tag, curr_bucket);
            curr_bucket = table_metadata[i].tag;
            curr_bucket_size_inc = curr_bucket_size + 1;
            curr_bucket_size = ObliviousChoose(new_bucket, 0, curr_bucket_size_inc);
            table_metadata[i].isExcess = ObliviousGreater(curr_bucket_size, main_bin_size);
        }
    }

    notify_threads(ThreadFn::osort_metadata);
    ObliviousSortParallel(table_metadata.begin(), table_metadata.begin()+placement_arr_len, cmp_req_table_key_metadata, num_threads, 0);
    wait_for_threads();

    // Do the same for nested hash table
    for (int i = 0; i < batch_sz; i++) {
        table_metadata[i].tag = table_metadata[i].tag2;
    }
    for (int i = 0; i < num_bins; i++) {
        for (int j = 0; j < main_bin_size; j++) {
            int idx = nested_table_start+batch_sz+i*main_bin_size+j;
            table_metadata[idx].tag = i;
            table_metadata[idx].isDummy = true;
        }
    }

    notify_threads(ThreadFn::osort_metadata2_pass1);
    ObliviousSortParallel(table_metadata.begin()+nested_table_start, table_metadata.begin()+nested_table_start+placement_arr_len, cmp_req_table_bin_assignment, num_threads, 0);
    wait_for_threads();
    {
        int curr_bucket_size = 0;
        int curr_bucket_size_inc;
        uint32_t curr_bucket = 0;
        bool new_bucket;
        for (int i = nested_table_start; i < nested_table_start+placement_arr_len; i++) {
            new_bucket = !ObliviousEqual(table_metadata[i].tag, curr_bucket);
            curr_bucket = table_metadata[i].tag;
            curr_bucket_size_inc = curr_bucket_size + 1;
            curr_bucket_size = ObliviousChoose(new_bucket, 0, curr_bucket_size_inc);
            table_metadata[i].isExcess = ObliviousGreater(curr_bucket_size, main_bin_size);
        }
    }

    notify_threads(ThreadFn::osort_metadata2);
    ObliviousSortParallel(table_metadata.begin()+nested_table_start, table_metadata.begin()+nested_table_start+placement_arr_len, cmp_req_table_key_metadata, num_threads, 0);
    wait_for_threads();

    for (int bkt = 0; bkt < num_buckets; bkt += BLOCK_BUF_BUCKETS) {
        fetch_from_queue_parallel();

        notify_threads(ThreadFn::assign_tags);
        _assign_tags(0);
        wait_for_threads();

        notify_threads(ThreadFn::process_blocks);
        _process_blocks(0);
        wait_for_threads();

        notify_threads(ThreadFn::process_blocks_h2);
        _process_blocks(0);
        wait_for_threads();

        notify_threads(ThreadFn::encrypt);
        _encrypt(0);
        wait_for_threads();
        for (int i = 0; i < BLOCK_BUF_BUCKETS; i++) {
            memcpy(&macs[(bkt+i)*TAG_LEN], buf_ct[i].tag, TAG_LEN);
        }
        s->sgx_q.write(buf_ct, BLOCK_BUF_BUCKETS);
    }

    for (int i = 0; i < combined_table_len; i++) {
        table_out[i].index = table_metadata[i].index;
        dummy_tags[i] = !table_metadata[i].isDummy;
    }
    ObliviousCompact(table_out.begin(), table_out.end(), dummy_tags.data());
    for (int i = 0; i < batch_sz; i++) {
        key_arr[i] = table_out[i].index;
        memcpy(out_data_arr[i], table_out[i].out_data, BLOCK_LEN);
    }
    //printf("num_buckets: %d, table_len: %d\n", num_table_buckets, table_len);
    //printf("sizeof table_metadata: %zu, table_out: %zu\n", table_metadata_len*sizeof(req_table_key_metadata), table_len*sizeof(req_table_val));

cleanup:
    notify_threads(ThreadFn::stop);
    for (auto ctx : key_ctxs) {
        EVP_CIPHER_CTX_free(ctx);
    }
    for (auto ctx : idx_key_ctxs) {
        EVP_CIPHER_CTX_free(ctx);
    }
    return rv;
}

int SuboramDispatcher::_prf(int thread_id) {
    int rv = ERROR;
    auto bounds = get_cutoffs_for_thread(thread_id, batch_sz, num_threads);
    EVP_CIPHER_CTX *key_ctx = key_ctxs[thread_id];
    uint32_t tag2 = 1 << 30;
    for (int i = bounds.first; i < bounds.second; i++) {
        uint32_t index = key_arr[i];
        table_metadata[i].index = index;
        CHECK_C (prf(key_ctx, (uint8_t *)&table_metadata[i].tag,
                        sizeof(uint32_t), (uint8_t *)&index, sizeof(uint32_t)));
        table_metadata[i].tag = table_metadata[i].tag % num_bins;
        index |= tag2;
        CHECK_C (prf(key_ctx, (uint8_t *)&table_metadata[i].tag2,
                        sizeof(uint32_t), (uint8_t *)&index, sizeof(uint32_t)));
        table_metadata[i].tag2 = table_metadata[i].tag2 % num_bins;
        memcpy(&table_metadata[i].in_data, in_data_arr[i], BLOCK_LEN);
        for (int j = 0; j < BLOCK_LEN; j++) {
            table_metadata[i].isRead &= ObliviousEqual(table_metadata[i].in_data[j], zero_u8);
        }
    }
    cleanup:
    return rv;
}

void SuboramDispatcher::_encrypt(int thread_id) {
    auto bounds = get_cutoffs_for_thread(thread_id, BLOCK_BUF_BUCKETS, num_threads);
    uint8_t *block_iv = block_ivs[thread_id];
    EVP_CIPHER_CTX *block_key_ctx = block_key_ctxs[thread_id];
    for (int i = 0; i < bounds.first; i++) {
        inc_iv(block_iv);
    }

    for (int i = bounds.first; i < bounds.second; i++) {
        buf[i].encrypt(block_key_ctx, block_iv, buf_ct[i]);
    }

    if (thread_id == 0) {
        for (int i = bounds.second; i < BLOCK_BUF_BUCKETS; i++) {
            inc_iv(block_iv);
        }
    }
}

void SuboramDispatcher::_process_blocks(int thread_id) {
    auto bounds = get_cutoffs_for_thread(thread_id, num_bins, num_threads);
    EVP_CIPHER_CTX *key_ctx = key_ctxs[thread_id];
    for (int buf_bkt = 0; buf_bkt < BLOCK_BUF_BUCKETS; buf_bkt++) {
        for (int i = 0; i < blocks_per_bucket; i++) {
            block *b = &buf[buf_bkt].items[i];
            uint32_t tag = buf_tags[buf_bkt*blocks_per_bucket+i];
            if (tag >= bounds.first && tag < bounds.second) {
                int start_idx = tag*main_bin_size;
                bool isMatch = false;
                /*
                bool useMatch = false;
                int idx = start_idx+rnd_idx[buf_bkt*blocks_per_bucket+i];
                */
                for (int j = start_idx; j < start_idx+main_bin_size; j++) {
                    req_table_key_metadata *km = &table_metadata[j];
                    isMatch = !km->isDummy && ObliviousEqual(b->pos, km->index);
                    /*
                    idx = ObliviousChoose(isMatch, j, idx);
                    */
                    req_table_val *out = &table_out[j];
                    obl::ObliviousBytesAssign(!km->isRead && isMatch, BLOCK_LEN, km->in_data, b->bytes, b->bytes);
                    obl::ObliviousBytesAssign(isMatch, BLOCK_LEN, out->out_data, b->bytes, out->out_data);
                }
                /*
                req_table_key_metadata *km = &table_metadata[idx];
                req_table_val *out = &table_out[idx];
                obl::ObliviousBytesAssign(!km->isRead && useMatch, BLOCK_LEN, km->in_data, b->bytes, b->bytes);
                obl::ObliviousBytesAssign(isMatch, BLOCK_LEN, out->out_data, b->bytes, out->out_data);
                */
            }
        }
    }
}

void SuboramDispatcher::_process_blocks_h2(int thread_id) {
    auto bounds = get_cutoffs_for_thread(thread_id, num_bins, num_threads);
    EVP_CIPHER_CTX *key_ctx = key_ctxs[thread_id];
    int total = 0;
    int processed = 0;
    for (int buf_bkt = 0; buf_bkt < BLOCK_BUF_BUCKETS; buf_bkt++) {
        for (int i = 0; i < blocks_per_bucket; i++) {
            block *b = &buf[buf_bkt].items[i];
            uint32_t tag2 = buf_tags2[buf_bkt*blocks_per_bucket+i];
            if (tag2 >= bounds.first && tag2 < bounds.second) {
                int start_idx = nested_table_start+tag2*main_bin_size;
                bool isMatch = false;
                /*
                bool useMatch = false;
                int idx = start_idx+rnd_idx2[buf_bkt*blocks_per_bucket+i];
                */
                for (int j = start_idx; j < start_idx+main_bin_size; j++) {
                    req_table_key_metadata *km = &table_metadata[j];
                    req_table_val *out = &table_out[j];
                    isMatch = !km->isDummy && ObliviousEqual(b->pos, km->index);
                    obl::ObliviousBytesAssign(!km->isRead && isMatch, BLOCK_LEN, km->in_data, b->bytes, b->bytes);
                    obl::ObliviousBytesAssign(isMatch, BLOCK_LEN, out->out_data, b->bytes, out->out_data);
                    /*
                    useMatch = useMatch || isMatch;
                    idx = ObliviousChoose(isMatch, j, idx);
                    */
                }
                /*
                req_table_key_metadata *km = &table_metadata[idx];
                req_table_val *out = &table_out[idx];
                obl::ObliviousBytesAssign(!km->isRead && useMatch, BLOCK_LEN, km->in_data, b->bytes, b->bytes);
                obl::ObliviousBytesAssign(isMatch, BLOCK_LEN, out->out_data, b->bytes, out->out_data);
                */
            }
        }
    }
}

int SuboramDispatcher::_assign_tags(int thread_id) {
    int rv = ERROR;
    auto bounds = get_cutoffs_for_thread(thread_id, BLOCK_BUF_BUCKETS, num_threads);
    EVP_CIPHER_CTX *key_ctx = key_ctxs[thread_id];
    EVP_CIPHER_CTX *idx_key_ctx = idx_key_ctxs[thread_id];
    uint32_t tag2_index = 1 << 30;
    uint32_t tag, tag2, idx1, idx2;
    for (int buf_bkt = bounds.first; buf_bkt < bounds.second; buf_bkt++) {
        for (int i = 0; i < blocks_per_bucket; i++) {
            block *b = &buf[buf_bkt].items[i];
            uint32_t index = b->pos;
            CHECK_C (prf(key_ctx, (uint8_t *)&tag,
                            sizeof(uint32_t), (uint8_t *)&index, sizeof(uint32_t)));
            tag = tag % num_bins;
            buf_tags[buf_bkt*blocks_per_bucket+i] = tag;

            /*
            CHECK_C (prf(idx_key_ctx, (uint8_t *)&idx1,
                            sizeof(uint32_t), (uint8_t *)&index, sizeof(uint32_t)));
            idx1 = idx1 % main_bin_size;
            rnd_idx[buf_bkt*blocks_per_bucket+i] = idx1;
            */

            index |= tag2_index;
            CHECK_C (prf(key_ctx, (uint8_t *)&tag2,
                            sizeof(uint32_t), (uint8_t *)&index, sizeof(uint32_t)));
            tag2 = tag2 % num_bins;
            buf_tags2[buf_bkt*blocks_per_bucket+i] = tag2;

            /*
            CHECK_C (prf(idx_key_ctx, (uint8_t *)&idx2,
                            sizeof(uint32_t), (uint8_t *)&index, sizeof(uint32_t)));
            idx2 = idx2 % main_bin_size;
            rnd_idx2[buf_bkt*blocks_per_bucket+i] = idx2;
            */
        }
    }
    cleanup:
    return rv;
}

int SuboramDispatcher::process_requests_thread(int thread_id) {
    int rv = ERROR;
    int next_iter = 1;
    while (true) {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&, this, next_iter, thread_id]{
            bool ready = state.curr_iter == next_iter;
            /*
            if (ready) {
                printf("[t%d] got job\n", thread_id);
            } else {
                printf("[t%d] waiting for job\n", thread_id);
            }
            */
            return ready;
        });
        lk.unlock();
        if (state.fn == ThreadFn::prf) {
            CHECK_C(_prf(thread_id));
        } else if (state.fn == ThreadFn::osort_metadata) {
            ObliviousSortParallel(table_metadata.begin(), table_metadata.begin()+placement_arr_len, cmp_req_table_key_metadata, num_threads, thread_id);
        } else if (state.fn == ThreadFn::osort_metadata_pass1) {
            ObliviousSortParallel(table_metadata.begin(), table_metadata.begin()+placement_arr_len, cmp_req_table_bin_assignment, num_threads, thread_id);
        } else if (state.fn == ThreadFn::osort_metadata2) {
            ObliviousSortParallel(table_metadata.begin()+nested_table_start, table_metadata.begin()+nested_table_start+placement_arr_len, cmp_req_table_key_metadata, num_threads, thread_id);
        } else if (state.fn == ThreadFn::osort_metadata2_pass1) {
            ObliviousSortParallel(table_metadata.begin()+nested_table_start, table_metadata.begin()+nested_table_start+placement_arr_len, cmp_req_table_bin_assignment, num_threads, thread_id);
        } else if (state.fn == ThreadFn::decrypt) {
            _decrypt(thread_id);
        } else if (state.fn == ThreadFn::process_blocks) {
            _process_blocks(thread_id);
        } else if (state.fn == ThreadFn::process_blocks_h2) {
            _process_blocks_h2(thread_id);
        } else if (state.fn == ThreadFn::encrypt) {
            _encrypt(thread_id);
        } else if (state.fn == ThreadFn::assign_tags) {
            CHECK_C(_assign_tags(thread_id));
        } else if (state.fn == ThreadFn::stop) {
            return rv;
        }
        next_iter++;
        lk.lock();
        if (++state.n_done == num_threads) {
            lk.unlock();
            cv.notify_all();
        }

    }
    cleanup:
    return rv;
}

void SuboramDispatcher::notify_threads(ThreadFn fn) {
    if (fn == ThreadFn::encrypt) {
        for (int i = 0; i < num_threads; i++) {
            memcpy(block_ivs[i], block_iv, IV_LEN);
        }
    }
    {
        std::lock_guard<std::mutex> lk(m);
        state.fn = fn;
        state.curr_iter++;
        state.n_done = 1;
    }
    cv.notify_all();
}

void SuboramDispatcher::wait_for_threads() {
    {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&, this]{
            bool done = state.n_done == num_threads;
            /*
            if (done) {
                printf("all threads done\n");
            } else {
                printf("waiting for threads to finish, done: %d\n", state.n_done);
            }
            */
            return done;
        });
        memcpy(block_iv, block_ivs[0], IV_LEN);
    }
}

// For mock suboram process_requests
void SuboramDispatcher::ecall_process_requests_parallel(int *ret, uint8_t *in_ct, uint8_t *in_iv, uint8_t *in_tag, uint8_t *out_ct, uint8_t *out_iv, uint8_t *out_tag, uint32_t batch_sz, int32_t balancer_id, int thread_id) {
    if (thread_id > 0) {
        *ret = process_requests_thread(thread_id);
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
    decrypt_key_val_pairs(comm_key, key_arr, in_data_arr, batch_sz, in_ct, in_iv, in_tag, &replay_ctr_in[balancer_id], true);
    //decrypt_key_val_pairs(dispatcher.comm_key[0], key_arr, in_data_arr, batch_sz, in_ct, in_iv, in_tag, &dispatcher.replay_ctr_in[balancer_id], true);

    process_requests_parallel(batch_sz, num_buckets, key_arr, in_data_arr, out_data_arr);

    CHECK_C (encrypt_key_val_pairs(comm_key, out_ct, out_iv, out_tag, key_arr, out_data_arr, batch_sz, &replay_ctr_out[balancer_id], true));
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
