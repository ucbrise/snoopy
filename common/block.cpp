#include <string.h>
#include <openssl/evp.h>
#include <vector>

#include "crypto.h"
#include "block.h"
#include "common.h"

using namespace std;

int get_key_val_buf_sz() {
    int sz = 2 * sizeof(uint32_t) + BLOCK_LEN;
    return sz % 16 == 0 ? sz : sz + (16 - (sz % 16));
}

int get_key_val_buf_sz(int num_pairs) {
    int sz = (2 * sizeof(uint32_t) + BLOCK_LEN) * num_pairs;
    return sz % 16 == 0 ? sz : sz + (16 - (sz % 16));
}

int encrypt_key_val(uint8_t *enc_key, uint8_t *bytesOut, uint8_t *iv,
        uint8_t *tag, uint32_t data_key, uint8_t *data, uint32_t *replay_ctr,
        bool immediate_inc) {
    int rv;
    uint8_t *buf;
    int sz = get_key_val_buf_sz();
    
    CHECK_A (buf = (uint8_t *)malloc(sz));
    memset(buf, 0, sz);
    memcpy(buf, (uint8_t *)replay_ctr, sizeof(uint32_t));
    memcpy(buf + sizeof(uint32_t), (uint8_t *)&data_key, sizeof(uint32_t));
    memcpy(buf + 2 * sizeof(uint32_t), data, BLOCK_LEN);
    CHECK_C (symm_encrypt(enc_key, bytesOut, iv, tag, buf, sz));
    if (immediate_inc != NULL) {
        *replay_ctr = *replay_ctr + 1;
    }
cleanup:
    if (buf) free(buf);
    return rv;
}

int encrypt_read_key(uint8_t *enc_key, uint8_t *bytesOut, uint8_t *iv,
        uint8_t *tag, uint32_t data_key, uint32_t *replay_ctr, bool immediate_inc) {
    int rv;
    uint8_t *buf;
    int sz = get_key_val_buf_sz();
    
    CHECK_A (buf = (uint8_t *)malloc(sz));
    memset(buf, 0, sz);
    memcpy(buf, (uint8_t *)replay_ctr, sizeof(uint32_t));
    memcpy(buf + sizeof(uint32_t), (uint8_t *)&data_key, sizeof(uint32_t));
    CHECK_C (symm_encrypt(enc_key, bytesOut, iv, tag, buf, sz));
    if (immediate_inc) {
        *replay_ctr = *replay_ctr + 1;
    }
cleanup:
    if (buf) free(buf);
    return rv;
}

int decrypt_key_val(uint8_t *dec_key, uint32_t *data_key, uint8_t *data,
        const uint8_t *bytesIn, const uint8_t *iv, uint8_t *tag, uint32_t *replay_ctr,
        bool immediate_inc) {
    int rv;
    uint8_t *buf;
    int sz = get_key_val_buf_sz();
    uint32_t ctr_received;

    CHECK_A (buf = (uint8_t *)malloc(sz));
    CHECK_C (symm_decrypt(dec_key, buf, iv, tag, bytesIn, sz));
    memcpy((uint8_t *)&ctr_received, buf, sizeof(uint32_t));
    memcpy((uint8_t *)data_key, buf + sizeof(uint32_t), sizeof(uint32_t));
    memcpy(data, buf + 2 * sizeof(uint32_t), BLOCK_LEN);
    if (ctr_received != *replay_ctr + 1) printf("*** counter FAIL: got %d expected %d\n", ctr_received, *replay_ctr + 1);
    CHECK_C (ctr_received == *replay_ctr + 1);
    if (immediate_inc) {
        *replay_ctr = *replay_ctr + 1;
    }
cleanup:
    if (buf) free(buf);
    return rv;
}

int encrypt_key_val_pairs(uint8_t *enc_key, uint8_t *bytesOut, uint8_t *iv,
        uint8_t *tag, uint32_t *data_key_arr, uint8_t **data_arr, int num_pairs,
        uint32_t *replay_ctr, bool immediate_inc) {
    int rv;
    uint8_t *buf;
    int total_len = get_key_val_buf_sz(num_pairs);
    int pair_len = sizeof(uint32_t) + BLOCK_LEN;

    CHECK_A (buf = (uint8_t *)(malloc(total_len)));
    memset(buf, 0, total_len);
    memcpy(buf, (uint8_t *)replay_ctr, sizeof(uint32_t));
    for (int i = 0; i < num_pairs; i++) {
        memcpy(buf + (i * pair_len) + sizeof(uint32_t), (uint8_t *)&data_key_arr[i], sizeof(uint32_t));
        memcpy(buf + (i * pair_len) + 2 * sizeof(uint32_t), data_arr[i], BLOCK_LEN);
    }
    CHECK_C (symm_encrypt(enc_key, bytesOut, iv, tag, buf, total_len));
    if (immediate_inc) {
        *replay_ctr = *replay_ctr + 1;
    }
cleanup:
    if (buf) free(buf);
    return rv;
}

int decrypt_key_val_pairs(uint8_t *dec_key, uint32_t *data_key_arr, uint8_t **data_arr,
        int num_pairs, const uint8_t *bytesIn, const uint8_t *iv, uint8_t *tag,
        uint32_t *replay_ctr, bool immediate_inc) {
    int rv;
    uint8_t *buf;
    int total_len = get_key_val_buf_sz(num_pairs);
    int pair_len = sizeof(uint32_t) + BLOCK_LEN;
    uint32_t ctr_received;

    CHECK_A (buf = (uint8_t *)malloc(total_len));
    CHECK_C (symm_decrypt(dec_key, buf, iv, tag, bytesIn, total_len));
    
    memcpy((uint8_t *)&ctr_received, buf, sizeof(uint32_t));
    for (int i = 0; i < num_pairs; i++) {
        memcpy((uint8_t *)&data_key_arr[i], buf + (i * pair_len) + sizeof(uint32_t), sizeof(uint32_t));
        memcpy(data_arr[i], buf + (i * pair_len) + 2 * sizeof(uint32_t), BLOCK_LEN);
    }
    if (ctr_received != *replay_ctr + 1) printf("*** counter FAIL: got %d expected %d\n", ctr_received, *replay_ctr + 1);
    CHECK_C (ctr_received == *replay_ctr + 1);
    if (immediate_inc) {
        *replay_ctr = *replay_ctr + 1;
    }
cleanup:
    if (buf) free(buf);
    return rv;
}

/* Dummy request constructor. */
namespace lb_types {
KeyBlockPair::KeyBlockPair() {
    key = -1;
    memset(block, 0, BLOCK_LEN);
    CID = -1;
    isResp = 0;
}
        
KeyBlockPair::KeyBlockPair(uint32_t key_in, uint8_t *block_in) {
    key = key_in;
    memcpy(block, block_in, BLOCK_LEN);
    CID = -1;
    isResp = 1;
}

KeyBlockPair::KeyBlockPair(uint32_t key_in, uint8_t *block_in, ClientID CID_in) {
    key = key_in;
    memcpy(block, block_in, BLOCK_LEN);
    CID = CID_in;
    isResp = 0;
}

KeyBlockPair::KeyBlockPair(const KeyBlockPair &x) {
    key = x.key;
    memcpy(block, x.block, BLOCK_LEN);
    CID = x.CID;
    isResp = x.isResp;
}

AssignedRequest::AssignedRequest() : SID(-1), tag(0) {}

AssignedRequest::AssignedRequest(SubOramID SID_in, uint8_t tag_in) : SID(SID_in), tag(tag_in) {}

AssignedRequest::AssignedRequest(KeyBlockPair &req_in, SubOramID SID_in, uint8_t tag_in) : req(req_in), SID(SID_in), tag(tag_in) {}
};

uint32_t get_suboram_for_req(EVP_CIPHER_CTX *key, uint32_t idx, uint32_t num_suborams) {
    // TODO: compute PRF_k mod num_suborams
    uint8_t tmp[4];
    uint32_t SID;
    prf(key, tmp, 4, (uint8_t *)&idx, sizeof(idx));
    SID = (tmp[0] << 24) + (tmp[1] << 16) + (tmp[2] << 8) + (tmp[3]);
    //suboram_key_map[SID % num_suborams].emplace(key);
    return SID % num_suborams;
}

uint32_t get_blocks_per_suboram(EVP_CIPHER_CTX *key, uint32_t num_suborams, uint32_t num_blocks) {
    vector<uint32_t> ctrs(num_suborams, 0);
    for (int i = 0; i < num_blocks; i++) {
        ctrs[get_suboram_for_req(key, i, num_suborams)]++;
    }
    uint32_t max = 0;
    for (int i = 0; i < num_suborams; i++) {
        if (ctrs[i] >= max) max = ctrs[i];
    }
    if (max % BLOCK_MULTIPLE == 0) {
        return max;
    }
    return max + (BLOCK_MULTIPLE - (max % BLOCK_MULTIPLE));
}
