#include "common.h"
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/bn.h>
#include <openssl/rand.h>
#include <string.h>
#include "crypto.h"
#include <assert.h>

inline int min (int a, int b) {
  return (a < b) ? a : b;
}

/*
 * Use SHA-256 to hash the string in `bytes_in`
 * with the integer given in `counter`.
 */
int hashOnce (EVP_MD_CTX *ctx, uint8_t *bytes_out,
    const uint8_t *bytes_in, int inlen, uint16_t counter)
{
  int rv = ERROR;
  CHECK_C (EVP_DigestInit_ex (ctx, EVP_sha256 (), NULL));
  CHECK_C (EVP_DigestUpdate (ctx, &counter, sizeof counter));
  CHECK_C (EVP_DigestUpdate (ctx, bytes_in, inlen));
  CHECK_C (EVP_DigestFinal_ex (ctx, bytes_out, NULL));

cleanup:
  return rv;
}

/*
 * Output a string of pseudorandom bytes by hashing a
 * counter with the bytestring provided:
 *    Hash(0|bytes_in) | Hash(1|bytes_in) | ...
 */
int hashToBytes (uint8_t *bytesOut, int outLen,
    const uint8_t *bytesIn, int inLen)
{
  int rv = ERROR;
  uint16_t counter = 0;
  uint8_t buf[SHA256_DIGEST_LENGTH];
  EVP_MD_CTX *ctx;

  ctx = EVP_MD_CTX_create();
  int bytesFilled = 0;
  do {
    const int toCopy = min (SHA256_DIGEST_LENGTH, outLen - bytesFilled);
    CHECK_C (hashOnce (ctx, buf, bytesIn, inLen, counter));
    memcpy (bytesOut + bytesFilled, buf, toCopy);

    counter++;
    bytesFilled += SHA256_DIGEST_LENGTH;
  } while (bytesFilled < outLen);

cleanup:
  if (ctx) EVP_MD_CTX_destroy(ctx);
  return rv;
}

int prf(EVP_CIPHER_CTX *ctx, uint8_t *bytesOut, int outLen,
        const uint8_t *bytesIn, int inLen) {
    int rv = ERROR;
    int bytesFilled = 0;
    uint8_t buf[16];
    uint8_t input[16];
    uint8_t counter = 0;

    // TODO: take in actual key and make ctx
    memset(input, 0, 16);
    memcpy(input + 1, bytesIn, min(inLen, 15));

    do {
        memcpy(input, &counter, sizeof(uint8_t));
        int bytesCopied;
        int toCopy = min(16, outLen - bytesFilled);
        CHECK_C (EVP_EncryptUpdate(ctx, buf, &bytesCopied, input, 16));
        memcpy(bytesOut + bytesFilled, buf, toCopy);
        bytesFilled += 16;
        counter++;
    } while (bytesFilled < outLen);

cleanup:
    return rv;
}

void inc_iv(uint8_t *iv) {
    for (int c = 0; c < IV_LEN; c++)
        if (++iv[c] != 0)
            break;
}

/* aadLen must be <= 16 */
/* bytesIn, aadLen = 16, outLen = 32 */
int symm_encrypt(const uint8_t *key, uint8_t *bytesOut,
        uint8_t *iv, uint8_t *tag, const uint8_t *bytesIn, int len) {
    int rv = ERROR;
    int bytesFilled = 0;
    EVP_CIPHER_CTX *ctx;
    int ct_len = 0;
    
    CHECK_C (RAND_bytes(iv, IV_LEN));

    CHECK_A (ctx = EVP_CIPHER_CTX_new());
    CHECK_C (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL));
    CHECK_C (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_LEN, NULL));
    CHECK_C (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv));
    //CHECK_C (EVP_EncryptUpdate(ctx, NULL, &bytesFilled, aad, AAD_LEN));
    CHECK_C (EVP_EncryptUpdate(ctx, bytesOut, &bytesFilled, bytesIn, len));
    ct_len = bytesFilled;
    CHECK_C (EVP_EncryptFinal_ex(ctx, bytesOut + ct_len, &bytesFilled));
    CHECK_C (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag));
cleanup:
    if (rv != OKAY) printf("NOT OK ENCRYPT\n");
    if (ctx) EVP_CIPHER_CTX_free(ctx);
    return rv;
}

int symm_encrypt(EVP_CIPHER_CTX *ctx, uint8_t *bytesOut,
        uint8_t *iv, uint8_t *tag, const uint8_t *bytesIn, uint8_t *curr_iv) {
    inc_iv(curr_iv);
    memcpy(iv, curr_iv, IV_LEN);
    int rv = ERROR;
    int bytesFilled = 0;
    int len = 0;
    //return OKAY;
    
    CHECK_C (EVP_EncryptInit_ex(ctx, NULL, NULL, NULL, iv));
    CHECK_C (EVP_EncryptUpdate(ctx, bytesOut, &bytesFilled, bytesIn, 16));
    len = bytesFilled;
    CHECK_C (EVP_EncryptFinal_ex(ctx, bytesOut + len, &bytesFilled));
    CHECK_C (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag));
cleanup:
    if (rv != OKAY) printf("NOT OK ENCRYPT\n");
    return rv;
}



int symm_decrypt(EVP_CIPHER_CTX *ctx, uint8_t *bytesOut,
        const uint8_t *iv, uint8_t *tag,
        const uint8_t *bytesIn) {
    int rv = ERROR;
    int bytesFilled = 0;
    //return OKAY;
    CHECK_C (EVP_DecryptInit_ex(ctx, NULL, NULL, NULL, iv));
    //CHECK_C (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL));
    //CHECK_C (EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv));
    CHECK_C (EVP_DecryptUpdate(ctx, bytesOut, &bytesFilled, bytesIn, 16));
    CHECK_C (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN, tag));
    CHECK_C (EVP_DecryptFinal_ex(ctx, bytesOut + bytesFilled, &bytesFilled));

cleanup:
    if (rv != OKAY) printf("NOT OK DECRYPT\n");
    return rv;
}

int symm_decrypt(const uint8_t *key, uint8_t *bytesOut,
        const uint8_t *iv, uint8_t *tag,
        const uint8_t *bytesIn, int len) {
    int rv = ERROR;
    int bytesFilled = 0;
    int ct_len = 0;
    EVP_CIPHER_CTX *ctx;
    CHECK_A (ctx = EVP_CIPHER_CTX_new());
    CHECK_C (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL));
    CHECK_C (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_LEN, NULL));
    CHECK_C (EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv));
    //CHECK_C (EVP_DecryptUpdate(ctx, NULL, &bytesFilled, aad, AAD_LEN));
    CHECK_C (EVP_DecryptUpdate(ctx, bytesOut, &bytesFilled, bytesIn, len));
    CHECK_C (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN, tag));
    ct_len = bytesFilled;
    CHECK_C (EVP_DecryptFinal_ex(ctx, bytesOut + ct_len, &bytesFilled));

cleanup:
    if (rv != OKAY) printf("NOT OK DECRYPT\n");
    if (ctx) EVP_CIPHER_CTX_free(ctx);
    return rv;
}

void print_bytes(char *label, const uint8_t *bytes, int len) {
    printf("%s: ", label);
    for (int i = 0; i < len; i++) {
        printf("%02x", bytes[i]);
    }
    printf("\n");
}

int encrypt_key_val(EVP_CIPHER_CTX *ctx, uint8_t **write_ptr,
        uint32_t key, uint8_t *data) {

}

int symm_encrypt(EVP_CIPHER_CTX *ctx, uint8_t *bytesOut,
        uint8_t *iv, uint8_t *tag, uint8_t *curr_iv, const uint8_t *bytesIn, int len) {
    inc_iv(curr_iv);
    memcpy(iv, curr_iv, IV_LEN);
    int rv = ERROR;
    int bytesFilled = 0;
    
    CHECK_C (EVP_EncryptInit_ex(ctx, NULL, NULL, NULL, iv));
    CHECK_C (EVP_EncryptUpdate(ctx, bytesOut, &bytesFilled, bytesIn, len));
    CHECK_C (EVP_EncryptFinal_ex(ctx, bytesOut + bytesFilled, &bytesFilled));
    CHECK_C (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag));
cleanup:
    if (rv != OKAY) printf("NOT OK ENCRYPT\n");
    return rv;
}

int symm_decrypt(EVP_CIPHER_CTX *ctx, uint8_t *bytesOut,
        const uint8_t *iv, uint8_t *tag, const uint8_t *bytesIn, int len) {
    int rv = ERROR;
    int bytesFilled = 0;
    CHECK_C (EVP_DecryptInit_ex(ctx, NULL, NULL, NULL, iv));
    CHECK_C (EVP_DecryptUpdate(ctx, bytesOut, &bytesFilled, bytesIn, len));
    CHECK_C (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN, tag));
    CHECK_C (EVP_DecryptFinal_ex(ctx, bytesOut + bytesFilled, &bytesFilled));

cleanup:
    if (rv != OKAY) printf("NOT OK DECRYPT\n");
    return rv;
}