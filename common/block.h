#ifndef _BLOCK_
#define _BLOCK_

#include <vector>
#include <openssl/evp.h>
#include "obl_primitives.h"
#include "common.h"
#include "crypto.h"

#define BLOCK_BUF_BUCKETS 64
#define BLOCKS_PER_BUCKET 64
#define BLOCK_MULTIPLE (BLOCK_BUF_BUCKETS*BLOCKS_PER_BUCKET)

typedef struct {
    uint8_t iv[IV_LEN];
    uint8_t tag[TAG_LEN];
    uint8_t bytes[BLOCK_LEN + 4];
    //uint8_t bytes[BLOCK_LEN + 24];
} block_ct;

typedef struct {
    uint32_t pos;
    uint8_t bytes[BLOCK_LEN];
    //uint8_t _padding[20];
} block;

int get_key_val_buf_sz();
int get_key_val_buf_sz(int num_pairs);

int encrypt_key_val(uint8_t *enc_key, uint8_t *bytesOut, uint8_t *iv,
        uint8_t *tag, uint32_t data_key, uint8_t *data, uint32_t *replay_ctr,
        bool immediate_inc);
int encrypt_read_key(uint8_t *enc_key, uint8_t *bytesOut, uint8_t *iv,
        uint8_t *tag, uint32_t data_key, uint32_t *replay_ctr,
        bool immediate_inc);
int decrypt_key_val(uint8_t *dec_key, uint32_t *data_key, uint8_t *data,
        const uint8_t *bytesIn, const uint8_t *iv, uint8_t *tag,
        uint32_t *replay_ctr, bool immediate_inc);

int encrypt_key_val_pairs(uint8_t *enc_key, uint8_t *bytesOut, uint8_t *iv,
        uint8_t *tag, uint32_t *data_key_arr, uint8_t **data_arr, int num_pairs,
        uint32_t *replay_ctr, bool immediate_inc);
int decrypt_key_val_pairs(uint8_t *dec_key, uint32_t *data_key_arr, uint8_t **data_arr,
        int num_pairs, const uint8_t *bytesIn, const uint8_t *iv, uint8_t *tag,
        uint32_t *replay_ctr, bool immediate_inc);

uint32_t get_suboram_for_req(EVP_CIPHER_CTX *key, uint32_t idx, uint32_t num_suborams);
uint32_t get_blocks_per_suboram(EVP_CIPHER_CTX *key, uint32_t num_suborams, uint32_t num_blocks);

template<class T>
struct bucket_item {
    uint32_t index;
    uint32_t bucket_sort_pos;
    T item;

    bucket_item() {}

    template<typename... Args>
    bucket_item(uint32_t index, Args... args) : index{index}, item(args...) {}
};

using block_bucket_item = bucket_item<block>;

template<typename T>
bool cmp_bucket_item_index(T a, T b) {
    static_assert(is_instantiation_of<T, bucket_item>::value, "Can only compare for a BucketItem");
    return ObliviousLess(a.index, b.index);
}

template<typename T>
bool cmp_bucket_item_pos(T a, T b) {
    static_assert(is_instantiation_of<T, bucket_item>::value, "Can only compare for a BucketItem");
    return ObliviousLess(a.bucket_sort_pos, b.bucket_sort_pos);
}

typedef struct {
    uint32_t tag;
    uint8_t in_data[BLOCK_LEN];
    uint8_t out_data[BLOCK_LEN];
    bool isRead;
    bool dummy = false;
} req_table_key;

using req_table_key_bucket_item = bucket_item<req_table_key>;

template<typename T>
class BucketCT {
    // static_assert(is_instantiation_of<T, bucket_item>::value, "Can only construct BucketCT for a BucketItem");

public:
    uint32_t num;
    uint8_t iv[IV_LEN];
    uint8_t tag[TAG_LEN];
    uint8_t *ct_bytes = NULL;
    int ct_len;

    int init(int z) {
        ct_len = sizeof(T) * z;
        ct_bytes = (uint8_t *) malloc(ct_len);
        return ct_bytes != NULL;
    }

    int decrypt(EVP_CIPHER_CTX *ctx, T *items) {
        return symm_decrypt(ctx, (uint8_t *) items, iv, tag, ct_bytes, ct_len);
    }
};

template<typename T>
class Bucket {
    // static_assert(is_instantiation_of<T, bucket_item>::value, "Can only construct Bucket for BucketItem");

    using BucketCTT = BucketCT<T>;

public:
    std::vector<T> items;
    int items_len;

    int init(int z) {
        items = std::vector<T>(z);
        items_len = sizeof(T) * z;
        return items.size() == z;
    }

    int encrypt(EVP_CIPHER_CTX *ctx, uint8_t *iv, BucketCTT &b_ct) {
        return symm_encrypt(ctx, b_ct.ct_bytes, b_ct.iv, b_ct.tag, iv, (uint8_t *) items.data(), items_len);
    }

    int encrypt(EVP_CIPHER_CTX *ctx, uint8_t *iv, BucketCTT &b_ct, int start, int len) {
        return symm_encrypt(ctx, b_ct.ct_bytes, b_ct.iv, b_ct.tag, iv, (uint8_t *) &items[start], sizeof(T) * len);
    }
};

// Datatypes sorted by the load balancer
namespace lb_types {
typedef uint32_t SubOramID;
typedef uint32_t ClientID;

class KeyBlockPair {
    public:
        uint32_t key;
        uint8_t block[BLOCK_LEN];     // NULL if read request
        ClientID CID;
        uint8_t isResp;

        KeyBlockPair();  // Dummy request constructor
        KeyBlockPair(uint32_t key_in, uint8_t *block_in);
        KeyBlockPair(uint32_t key_in, uint8_t *block_in, ClientID CID_in);
        KeyBlockPair(const KeyBlockPair &x);
};

using KeyBlockPairBucketItem = bucket_item<KeyBlockPair>;

struct KeyBlockPairBucketItemSorter {
    bool operator()(const KeyBlockPairBucketItem a, const KeyBlockPairBucketItem b) {
        uint64_t a_64 = ((uint64_t)a.item.key << 32) | ((1 - a.item.isResp) % 2);
        uint64_t b_64 = ((uint64_t)b.item.key << 32) | ((1 - b.item.isResp) % 2);
        return ObliviousLess(a_64, b_64);
    }
};

class AssignedRequest {
    public:
        KeyBlockPair req;
        SubOramID SID;
        uint8_t tag;

        AssignedRequest();
        AssignedRequest(SubOramID SID_in, uint8_t tag_in);
        AssignedRequest(KeyBlockPair &req_in, SubOramID SID_in, uint8_t tag_in);
};

using AssignedRequestBucketItem = bucket_item<AssignedRequest>;

struct AssignedRequestBucketItemSorter {
    bool operator()(const AssignedRequestBucketItem a, const AssignedRequestBucketItem b) {
        uint64_t a_64 = ((uint64_t)a.item.SID << 32) | a.item.tag;
        uint64_t b_64 = ((uint64_t)b.item.SID << 32) | b.item.tag;
        return ObliviousLess(a_64, b_64);
    }
};
};

#endif
