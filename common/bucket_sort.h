#pragma once

#include <algorithm>
#include <math.h>
#include <iostream>
#include <stdio.h>
#include <utility>
#include <tuple>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <openssl/rand.h>

#include "common.h"
#include "crypto.h"
#include "block.h"
#include "obl_primitives.h"
#include "ring_buffer.h"

#define BUF_BUCKETS 128

template<typename EnclaveFn, typename... Args>
std::thread spawn_enclave_thread(EnclaveFn&& enc_fn, Args&&... args) {
    std::thread enclave_thread([](typename std::decay<EnclaveFn>::type&& enc_fn, typename std::decay<Args>::type&&... args){
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        std::cout << "Enclave thread on CPU " << sched_getcpu() << "\n";
        enc_fn(std::move(args)...);
    }, std::forward<EnclaveFn>(enc_fn), std::forward<Args>(args)...);
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    int rc = pthread_setaffinity_np(enclave_thread.native_handle(), sizeof(cpuset), &cpuset);
    if (rc != 0) {
        std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
    }
    return enclave_thread;
}

template<typename EnclaveFn, typename... Args>
std::thread spawn_enclave_thread_on_cpu(int thread_idx, EnclaveFn&& enc_fn, Args&&... args) {
    std::thread enclave_thread([](typename std::decay<EnclaveFn>::type&& enc_fn, typename std::decay<Args>::type&&... args){
        //std::this_thread::sleep_for(std::chrono::milliseconds(5));
        // std::cout << "Enclave thread on CPU " << sched_getcpu() << "\n";
        enc_fn(std::move(args)...);
    }, std::forward<EnclaveFn>(enc_fn), std::forward<Args>(args)...);
    /*
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread_idx, &cpuset);
    int rc = pthread_setaffinity_np(enclave_thread.native_handle(), sizeof(cpuset), &cpuset);
    if (rc != 0) {
        std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
    }
    */
    return enclave_thread;
}

void pin_host_thread();

class BucketSortParams {
public:
    int num_blocks;
    int log_num_blocks;
    int z;
    int log_total_buckets;
    uint32_t total_buckets;
    size_t buf_size;

    BucketSortParams();
    BucketSortParams(int n);

private:
    int get_z(int n);
};

template<typename T>
struct shared_sort_state {
    RingBuffer<BucketCT<T>> host_q;
    RingBuffer<BucketCT<T>> sgx_q;

    int num_blocks;
    bool ready = false;
    bool resized = false;
};

namespace bs {
    enum ThreadFn { 
        decrypt,
        insert_dummies,
        merge_split_curr_buffer,
        process_last_layer_buffer,
        stop,
    };

    struct thread_state{
        ThreadFn fn;
        int curr_iter = 0;
        int done;
        int batch;
        int i;
        std::vector<uint8_t *> tags;
        std::vector<uint8_t *> tmp;
        std::vector<int> bucket_lens;
        std::vector<EVP_CIPHER_CTX *> prf_ctx;
    };

    extern std::mutex m;
    extern std::condition_variable cv;
    extern thread_state state;
}

template<typename T>
class EnclaveBucketSorter {
    static_assert(is_instantiation_of<T, bucket_item>::value, "Can only sort on BucketItem");

    using BucketT = Bucket<T>;
    using BucketCTT = BucketCT<T>;

private:
    uint8_t *curr_macs;
    uint8_t *next_macs;
    int bucket_idx;
    BucketCTT buf_ct[BUF_BUCKETS];
    BucketT buf[BUF_BUCKETS / 2];
    BucketSortParams params;
    uint8_t *aes_key;
    uint8_t *iv;
    shared_sort_state<T> *s;

    std::vector<EVP_CIPHER_CTX *> aes_ctxs;
    std::vector<uint8_t *> ivs;
    std::vector<T> *blocks_p;

    int n_threads;

public:
    EnclaveBucketSorter() {}
    EnclaveBucketSorter(shared_sort_state<T> *s, uint8_t *aes_key, uint8_t *iv, int n_threads = 1) : s(s), aes_key(aes_key), iv(iv), n_threads(n_threads) {}

    int init_bucket_sort() {
        int rv = ERROR;
        EVP_CIPHER_CTX *aes_ctx;
        uint8_t *thread_iv;
        for (int i = 0; i < BUF_BUCKETS / 2; i++) {
            CHECK_C(buf[i].init(2*params.z));
        }

        for (int i = 0; i < n_threads; i++) {
            CHECK_A (aes_ctx = EVP_CIPHER_CTX_new());
            CHECK_C (EVP_EncryptInit_ex(aes_ctx, EVP_aes_256_gcm(), NULL, aes_key, NULL));
            aes_ctxs.push_back(aes_ctx);
            CHECK_A (thread_iv = (uint8_t *) malloc(sizeof(uint8_t)*IV_LEN));
            memcpy(thread_iv, iv, IV_LEN);
            ivs.push_back(thread_iv);
        }

        bs::state.bucket_lens = std::vector<int>(BUF_BUCKETS);
        cleanup:
        return rv;
    }

    int resize(int num_blocks) {
        int rv = ERROR;
        params = BucketSortParams(num_blocks);
        for (int i = 0; i < BUF_BUCKETS / 2; i++) {
            CHECK_C(buf[i].init(2*params.z));
        }
        bucket_idx = 0;
        CHECK_A (curr_macs = (uint8_t *) malloc(TAG_LEN * params.total_buckets));
        CHECK_A (next_macs = (uint8_t *) malloc(TAG_LEN * params.total_buckets));
        cleanup:
        return rv;
    }

    template<typename Comparator>
    int sort(std::vector<T> &blocks, Comparator cmp, int thread_idx = 0) {
        this->blocks_p = &blocks;
        if (thread_idx > 0) {
            return buffered_bucket_sort_thread(thread_idx);
        }
        bs::state.prf_ctx.clear();
        int num_blocks = blocks.size();
        if (num_blocks != params.num_blocks) {
            resize(num_blocks);
        }
        s->num_blocks = num_blocks;
        s->resized = false;
        s->ready = true;
        while (!s->resized) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        int rv = ERROR;
        int log_total_buckets_bytes = ceil((float) params.log_total_buckets / 8);

        int real_z = params.num_blocks / params.total_buckets;
        unsigned char *key;
        printf("levels: %d, z: %d, buckets: %d, blocksize: %zu\n", params.log_total_buckets, params.z, params.total_buckets, sizeof(T));
        uint8_t *tmp;
        bs::state.tmp.clear();
        for (int i = 0; i < n_threads; i++) {
            CHECK_A (tmp = (uint8_t *) malloc(log_total_buckets_bytes * sizeof(uint8_t)));
            bs::state.tmp.push_back(tmp);
        }
        CHECK_A (tmp = (uint8_t *) malloc(log_total_buckets_bytes * sizeof(uint8_t)));
        CHECK_A (key = (unsigned char *) malloc(16 * sizeof(unsigned char)));
        {
        RAND_bytes(key, 16);
        EVP_CIPHER_CTX *prf_ctx;
        for (int i = 0; i < n_threads; i++) {
            EVP_CIPHER_CTX *prf_ctx = EVP_CIPHER_CTX_new();
            EVP_EncryptInit_ex(prf_ctx, EVP_aes_128_ecb(), NULL, key, NULL);
            bs::state.prf_ctx.push_back(prf_ctx);
        }
        prf_ctx = EVP_CIPHER_CTX_new();
        EVP_EncryptInit_ex(prf_ctx, EVP_aes_128_ecb(), NULL, key, NULL);
        for (auto &b : blocks) {
            assign_random_pos(prf_ctx, log_total_buckets_bytes, params.log_total_buckets, &b, tmp);
        }
        for (uint32_t batch = 0; batch < params.total_buckets / BUF_BUCKETS; batch++) {
            fetch_ct_from_queue();
            bs::state.batch = batch;
            notify_threads(bs::ThreadFn::insert_dummies);
            insert_dummies(0);
            wait_for_threads();
            s->sgx_q.write(buf_ct, BUF_BUCKETS);
        }
        uint8_t *tags;
        bs::state.tags.clear();
        for (int i = 0; i < n_threads; i++) {
            CHECK_A (tags = (uint8_t *) malloc(2*params.z*sizeof(uint8_t)));
            bs::state.tags.push_back(tags);
        }
        CHECK_A (tags = (uint8_t *) malloc(2*params.z*sizeof(uint8_t)));
        for (int i = 0; i < params.log_total_buckets - 1; i++) {
            bs::state.i = i;
            int stride_len = 1 << (i + 2);
            int half_stride = 1 << (i + 1);
            int stride_ct = 0;
            int stride = 0;

            for (int j = 0; j < params.total_buckets / BUF_BUCKETS; j++) {
                fetch_from_queue();
                notify_threads(bs::ThreadFn::merge_split_curr_buffer);
                merge_split_curr_buffer(0);
                wait_for_threads();
                for (int k = 0; k < BUF_BUCKETS / 2; k++) {
                    for (int l = 0; l < 2; l++) {
                        int idx = (stride * stride_len) + (stride_ct%half_stride)*2;
                        if (stride_ct < half_stride) { // lhs
                            memcpy(&next_macs[idx*TAG_LEN], &buf_ct[2*k+l].tag, TAG_LEN);
                        } else { // rhs
                            memcpy(&next_macs[(idx+1)*TAG_LEN], &buf_ct[2*k+l].tag, TAG_LEN);
                        }
                        stride_ct++;
                        if (stride_ct == stride_len) {
                            stride_ct = 0;
                            stride++;
                        }
                    }
                }
                s->sgx_q.write(buf_ct, BUF_BUCKETS);
            }
            std::swap(curr_macs, next_macs);
        }

        int block_idx = 0;
        int bucket_end = 0;
        for (int j = 0; j < params.total_buckets / BUF_BUCKETS; j++) {
            fetch_from_queue();
            notify_threads(bs::ThreadFn::process_last_layer_buffer);
            process_last_layer_buffer(0);
            wait_for_threads();
            for (int k = 0; k < BUF_BUCKETS / 2; k++) {
                bucket_end = bs::state.bucket_lens[k*2];
                memcpy(&blocks[block_idx], &buf[k].items[0], sizeof(block) * bucket_end);
                block_idx += bucket_end;
                bucket_end = bs::state.bucket_lens[k*2+1];
                memcpy(&blocks[block_idx], &buf[k].items[0], sizeof(block) * bucket_end);
                block_idx += bucket_end;
            }
            s->sgx_q.write(buf_ct, BUF_BUCKETS);
        }
        /*
        for (int j = 0; j < params.total_buckets / BUF_BUCKETS; j++) {
            fetch_from_queue();
            for (int k = 0; k < BUF_BUCKETS / 2; k++) {
                merge_split(&buf[k].blocks[0], &buf[k].blocks[params.z], buf[k].blocks, tags, params.z, params.log_total_buckets, 0, params.log_total_buckets);
                for (int i = 0; i < 2*params.z; i++) {
                    tags[i] = (bool) (buf[k].blocks[i].bucket_sort_pos & params.total_buckets);
                    tags[i] = 1 - tags[i];
                }
                ObliviousCompact(buf[k].blocks.begin(), buf[k].blocks.begin()+params.z, tags);
                ObliviousCompact(buf[k].blocks.begin()+params.z, buf[k].blocks.begin()+2*params.z, &tags[params.z]);
                bucket_end = 0;
                for (int i = 0; i < params.z; i++) {
                    if(buf[k].blocks[i].bucket_sort_pos >= params.total_buckets) {
                        bucket_end = i;
                        break;
                    }
                    assign_random_pos(prf_ctx, log_total_buckets_bytes, params.log_num_blocks, &buf[k].blocks[i], tmp);
                }
                memcpy(&blocks[block_idx], &buf[k].blocks[0], sizeof(block) * bucket_end);
                ObliviousSort(blocks.begin() + block_idx, blocks.begin() + block_idx + bucket_end, _cmp_block_pos);
                block_idx += bucket_end;
                bucket_end = 0;
                for (int i = params.z; i < 2*params.z; i++) {
                    if(buf[k].blocks[i].bucket_sort_pos >= params.total_buckets) {
                        bucket_end = i-params.z;
                        break;
                    }
                    assign_random_pos(prf_ctx, log_total_buckets_bytes, params.log_num_blocks, &buf[k].blocks[i], tmp);
                }
                memcpy(&blocks[block_idx], &buf[k].blocks[params.z], sizeof(block) * bucket_end);
                ObliviousSort(blocks.begin() + block_idx, blocks.begin() + block_idx + bucket_end, _cmp_block_pos);
                block_idx += bucket_end;
            }
            s->sgx_q.write(buf_ct, BUF_BUCKETS);
        }
        */
        //printf("thread 0: stopping\n");
        notify_threads(bs::ThreadFn::stop);
        std::sort(blocks.begin(), blocks.end(), cmp);
        }
        cleanup:
        return OKAY;
    }

private:
    void assign_random_pos(EVP_CIPHER_CTX *prf_ctx, int num_bytes, int num_bits, T *b, uint8_t *tmp) {
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

    void merge_split(T *input0, T *input1, std::vector<T> &input,
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

    int buffered_bucket_sort_thread(int thread_id) {
        int next_iter= 1;
        while(true) {
            std::unique_lock<std::mutex> lk(bs::m);
            bs::cv.wait(lk, [this, next_iter, thread_id]{
                bool ready = bs::state.curr_iter == next_iter;
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
            bs::ThreadFn fn = bs::state.fn;
            int local_bucket_idx = bucket_idx;
            //bs::cv.notify_all();
            int real_z = params.num_blocks / params.total_buckets;
            if (fn == bs::ThreadFn::decrypt) {
                decrypt_buffer(thread_id);
            } else if (fn == bs::ThreadFn::insert_dummies) {
                insert_dummies(thread_id);
            } else if (fn == bs::ThreadFn::merge_split_curr_buffer) {
                merge_split_curr_buffer(thread_id);
            } else if (fn == bs::ThreadFn::process_last_layer_buffer) {
                process_last_layer_buffer(thread_id);
            } else if (fn == bs::ThreadFn::stop) {
                //printf("thread %d: stopping\n", idx);
                return OKAY;
            }
            next_iter++;
            lk.lock();
            if (++bs::state.done == n_threads) {
                lk.unlock();
                bs::cv.notify_all();
            }
        }
    }

    int fetch_ct_from_queue() {
        volatile int read = 0;
        int i = 0;
        while (read <= 0) {
            read = s->host_q.read_full(buf_ct, BUF_BUCKETS);
            i++;
        }
        //printf("sgx waits: %d\n", i);
        return OKAY;
    }

    int fetch_from_queue() {
        fetch_ct_from_queue();
        notify_threads(bs::ThreadFn::decrypt);
        decrypt_buffer(0);
        wait_for_threads();
        bucket_idx = (bucket_idx + BUF_BUCKETS) % params.total_buckets;
        return OKAY;
    }

    void decrypt_buffer(int thread_id) {
        auto bounds = get_cutoffs_for_thread(thread_id, BUF_BUCKETS / 2);
        int local_bucket_idx = (bucket_idx + 2*bounds.first) % params.total_buckets;
        // printf("[t%d] [idx %d] [strt %d]\n", thread_id, local_bucket_idx, bucket_idx);
        for (int i = bounds.first; i < bounds.second; i++) {
            //printf("thread: %d, local_bucket_idx: %d\n", thread_id, local_bucket_idx);
            for (int j = 0; j < 2; j++) {
                uint8_t* expected_mac = &curr_macs[local_bucket_idx*TAG_LEN];
                for (int t = 0; t < TAG_LEN; t++) {
                    if (expected_mac[t] != buf_ct[2*i+j].tag[t]) {
                        printf("[t%d] [i %d, j %d] [idx %d] [strt %d] TAG DOES NOT MATCH:\n\texpected 0x", thread_id, i, j, local_bucket_idx, bucket_idx);
                        for (int t = 0; t < TAG_LEN; t++) {
                            printf(" %02x ", expected_mac[t]);
                        }
                        printf("\n\tbut got  0x");
                        for (int t = 0; t < TAG_LEN; t++) {
                            printf(" %02x ", buf_ct[2*i+j].tag[t]);
                        }
                        printf("\n");
                        break;
                    }
                }
                buf_ct[i*2+j].decrypt(aes_ctxs[thread_id], &buf[i].items[j*params.z]);
                local_bucket_idx = (local_bucket_idx + 1) % (params.total_buckets);
            }
        }
    }

    void insert_dummies(int thread_id) {
        int real_z = params.num_blocks / params.total_buckets;
        auto bounds = get_cutoffs_for_thread(thread_id, BUF_BUCKETS / 2);
        EVP_CIPHER_CTX *aes_ctx = aes_ctxs[thread_id];
        uint8_t *iv = ivs[thread_id];
        for (int i = 0; i < bounds.first; i++) {
            inc_iv(iv);
            inc_iv(iv);
        }
        for (int k = bounds.first; k < bounds.second; k++) {
            for (int l = 0; l < 2; l++) {
                int b = bs::state.batch * BUF_BUCKETS + 2*k + l;
                memcpy(&buf[k].items[params.z*l], &blocks_p->operator[](b * real_z), sizeof(T)*real_z);
                for (int i = real_z; i < params.z; i++) {
                    buf[k].items[params.z*l+i].bucket_sort_pos = params.total_buckets;
                }
                buf[k].encrypt(aes_ctx, iv, buf_ct[2*k+l], params.z*l, params.z);
                memcpy(&curr_macs[b*TAG_LEN], buf_ct[2*k+l].tag, TAG_LEN);
                buf_ct[2*k+l].num = b;
            }
        }
        if (thread_id == 0) {
            for (int i = bounds.second; i < BUF_BUCKETS / 2; i++) {
                inc_iv(iv);
                inc_iv(iv);
            }
        }
    }

    void merge_split_curr_buffer(int thread_id) {
        auto bounds = get_cutoffs_for_thread(thread_id, BUF_BUCKETS / 2);
        uint8_t *tags = bs::state.tags[thread_id];
        EVP_CIPHER_CTX *aes_ctx = aes_ctxs[thread_id];
        uint8_t *iv = ivs[thread_id];
        for (int i = 0; i < bounds.first; i++) {
            inc_iv(iv);
            inc_iv(iv);
        }
        for (int k = bounds.first; k < bounds.second; k++) {
            merge_split(&buf[k].items[0], &buf[k].items[params.z], buf[k].items, tags, params.z, bs::state.i, 0, params.log_total_buckets);
            for (int l = 0; l < 2; l++) {
                buf[k].encrypt(aes_ctx, iv, buf_ct[2*k+l], params.z*l, params.z);
            }
        }
        if (thread_id == 0) {
            for (int i = bounds.second; i < BUF_BUCKETS / 2; i++) {
                inc_iv(iv);
                inc_iv(iv);
            }
        }
    }

    void process_last_layer_buffer(int thread_id) {
        int log_total_buckets_bytes = ceil((float) params.log_total_buckets / 8);
        auto bounds = get_cutoffs_for_thread(thread_id, BUF_BUCKETS / 2);
        uint8_t *tags = bs::state.tags[thread_id];
        uint8_t *tmp = bs::state.tmp[thread_id];
        EVP_CIPHER_CTX *prf_ctx = bs::state.prf_ctx[thread_id];
        for (int k = bounds.first; k < bounds.second; k++) {
            int bucket_end = params.z / 2;
            merge_split(&buf[k].items[0], &buf[k].items[params.z], buf[k].items, tags, params.z, params.log_total_buckets, 0, params.log_total_buckets);
            for (int i = 0; i < 2*params.z; i++) {
                tags[i] = (bool) (buf[k].items[i].bucket_sort_pos & params.total_buckets);
                tags[i] = 1 - tags[i];
            }
            ObliviousCompact(buf[k].items.begin(), buf[k].items.begin()+params.z, tags);
            ObliviousCompact(buf[k].items.begin()+params.z, buf[k].items.begin()+2*params.z, &tags[params.z]);
            for (int i = 0; i < bucket_end; i++) {
                // printf("pos: %d\n", buf[k].items[i].bucket_sort_pos);
                if(buf[k].items[i].bucket_sort_pos >= params.total_buckets) {
                    bucket_end = i;
                    break;
                }
                assign_random_pos(prf_ctx, log_total_buckets_bytes, params.log_num_blocks, &buf[k].items[i], tmp);
            }
            for (int i = bucket_end; i < params.z; i++) {
                buf[k].items[i].bucket_sort_pos = params.total_buckets;
            }
            ObliviousSort(buf[k].items.begin(), buf[k].items.begin()+bucket_end, cmp_bucket_item_pos<T>);
            // memcpy(&blocks[block_idx], &buf[k].items[0], sizeof(block) * bucket_end);
            // block_idx += bucket_end;
            bs::state.bucket_lens[k*2] = bucket_end;
            bucket_end = 0;
            for (int i = params.z; i < params.z + bucket_end; i++) {
                if(buf[k].items[i].bucket_sort_pos >= params.total_buckets) {
                    bucket_end = i-params.z;
                    break;
                }
                assign_random_pos(prf_ctx, log_total_buckets_bytes, params.log_num_blocks, &buf[k].items[i], tmp);
            }
            for (int i = params.z+bucket_end; i < 2*params.z; i++) {
                buf[k].items[i].bucket_sort_pos = params.total_buckets;
            }
            ObliviousSort(buf[k].items.begin()+params.z, buf[k].items.begin()+params.z+bucket_end, cmp_bucket_item_pos<T>);
            //memcpy(&blocks[block_idx], &buf[k].items[params.z], sizeof(block) * bucket_end);
            //block_idx += bucket_end;
            bs::state.bucket_lens[k*2+1] = bucket_end;
        }
    }

    void notify_threads(bs::ThreadFn fn) {
        if (n_threads == 1) {
            return;
        }
        if (fn == bs::ThreadFn::insert_dummies || fn == bs::ThreadFn::merge_split_curr_buffer) {
            for (int i = 0; i < n_threads; i++) {
                memcpy(ivs[i], iv, IV_LEN);
            }
        }
        {
            std::lock_guard<std::mutex> lk(bs::m);
            bs::state.fn = fn;
            bs::state.curr_iter++;
            bs::state.done = 1;
        }
        bs::cv.notify_all();
    }

    void wait_for_threads() {
        if (n_threads == 1) {
            return;
        }
        {
            std::unique_lock<std::mutex> lk(bs::m);
            bs::cv.wait(lk, [this]{
                bool done = bs::state.done == n_threads;
                /*
                if (done) {
                    printf("all threads done\n");
                } else {
                    printf("waiting for threads to finish, done: %d\n", bs::state.done);
                }
                */
                return done;
            });
        }
        memcpy(ivs[0], iv, IV_LEN);
    }

    std::pair<int, int> get_cutoffs_for_thread(int thread_id, int total) {
        int chunks = floor(total / n_threads);
        int start = chunks*thread_id;
        int end = start+chunks;
        if (thread_id + 1 == n_threads) {
            end = total;
        }
        //printf("[t %d] bounds: [%d, %d)\n", thread_id, start, end);
        return std::make_pair(start, end);
    }
};

template<typename T>
class HostBucketSorter {
    static_assert(is_instantiation_of<T, bucket_item>::value, "Can only sort on BucketItem");

    using BucketT = Bucket<T>;
    using BucketCTT = BucketCT<T>;

private:
    BucketSortParams params;
    BucketCTT buf[BUF_BUCKETS];
    std::vector<BucketCTT> curr_level;
    std::vector<BucketCTT> next_level;

public:
    shared_sort_state<T> s;

    HostBucketSorter() {}
    /*
    HostBucketSorter(int num_buckets) : params(num_buckets), host_q(params.buf_size), sgx_q(params.buf_size) {
        for (int i = 0; i < params.total_buckets; i++) {
            BucketCTT b_ct;
            if (b_ct.init(params.z) != OKAY) {
                printf("failed to init bucket ct\n");
                return;
            }
            curr_level.push_back(b_ct);
        }
        for (int i = 0; i < params.total_buckets; i++) {
            BucketCTT b_ct;
            if (b_ct.init(params.z) != OKAY) {
                printf("failed to init bucket ct\n");
                return;
            }
            next_level.push_back(b_ct);
        }
    }
    */

    void resize(int num_buckets) {
        params = BucketSortParams(num_buckets);
        s.host_q.resize(params.buf_size);
        s.sgx_q.resize(params.buf_size);
        for (int i = curr_level.size(); i < params.total_buckets; i++) {
            BucketCTT b_ct;
            if (b_ct.init(params.z) != OKAY) {
                printf("failed to init bucket ct\n");
                return;
            }
            curr_level.push_back(b_ct);
        }
        for (int i = next_level.size(); i < params.total_buckets; i++) {
            BucketCTT b_ct;
            if (b_ct.init(params.z) != OKAY) {
                printf("failed to init bucket ct\n");
                return;
            }
            next_level.push_back(b_ct);
        }
    }

    template<typename Comparator>
    void mock_enclave_sort(std::vector<T> &blocks, Comparator cmp, int n_threads) {
        uint8_t *aes_key = (uint8_t *) "0123456789123456"; // TODO: Change keys
        uint8_t iv[IV_LEN] = {0};
        EnclaveBucketSorter<T> enc_sorter(&s, aes_key, iv, n_threads);
        enc_sorter.init_bucket_sort();
        std::thread host_thread([this] () mutable {
            // pin_host_thread();
            fill_buffer();
        });
        std::vector<std::thread> threads;
        for (int i = 0; i < n_threads; i++) {
            threads.push_back(std::thread ([cmp] (std::vector<T> &blocks, EnclaveBucketSorter<T> &enc_sorter, int thread_id) {
                // std::this_thread::sleep_for(std::chrono::milliseconds(20));
                std::cout << "Mock enclave thread on CPU " << sched_getcpu() << "\n";
                enc_sorter.sort(blocks, cmp, thread_id);
            }, std::ref(blocks), std::ref(enc_sorter), i));
        }
        for (int i = 0; i < n_threads; i++) {
            threads[i].join();
        }
        host_thread.join();
    }

    void fill_buffer() {
        while (!s.ready) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (s.num_blocks != params.num_blocks) {
            resize(s.num_blocks);
        }
        s.ready = false;
        s.resized = true;

        int n_batches = params.total_buckets / BUF_BUCKETS;
        s.host_q.write(curr_level.data(), BUF_BUCKETS);
        for (uint32_t batch = 0; batch < params.total_buckets / BUF_BUCKETS; batch++) {
            if (batch < n_batches - 1) {
                s.host_q.write(&curr_level[(batch+1)*BUF_BUCKETS], BUF_BUCKETS);
            }
            fetch_from_queue();
            memcpy(&curr_level[batch*BUF_BUCKETS], buf, sizeof(BucketCTT)*BUF_BUCKETS);
        }

        for (int i = 0; i < params.log_total_buckets - 1; i++) {
            int b = 0;
            int stride_len = 1 << (i + 2);
            int half_stride = 1 << (i + 1);
            int stride_ct = 0;
            int stride = 0;
            s.host_q.write(curr_level.data(), BUF_BUCKETS);
            for (int batch = 0; batch < params.total_buckets / BUF_BUCKETS; batch++) {
                if (batch < n_batches - 1) {
                    s.host_q.write(&curr_level[(batch+1)*BUF_BUCKETS], BUF_BUCKETS);
                }
                fetch_from_queue();
                for (int j = 0; j < BUF_BUCKETS; j++) {
                    int idx = (stride * stride_len) + (stride_ct%half_stride)*2;
                    if (stride_ct < half_stride) { // lhs
                        memcpy(&next_level[idx], &buf[j], sizeof(BucketCTT));
                    } else { // rhs
                        memcpy(&next_level[idx+1], &buf[j], sizeof(BucketCTT));
                    }
                    stride_ct++;
                    if (stride_ct == stride_len) {
                        stride_ct = 0;
                        stride++;
                    }
                }
            }
            curr_level.swap(next_level);
        }

        s.host_q.write(curr_level.data(), BUF_BUCKETS);
        for (int batch = 0; batch < params.total_buckets / BUF_BUCKETS; batch++) {
            if (batch < n_batches - 1) {
                s.host_q.write(&curr_level[(batch+1)*BUF_BUCKETS], BUF_BUCKETS);
            }
            fetch_from_queue();
        }
    }

private:
    void fetch_from_queue() {
        volatile int read = 0;
        int i = 0;
        while (read <= 0) {
            read = s.sgx_q.read_full(buf, BUF_BUCKETS);
            i++;
        }
        //printf("host waits: %d\n", i);
    }
};