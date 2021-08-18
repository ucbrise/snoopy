#ifndef _CRYPTO_
#define _CRYPTO_

#include <openssl/evp.h>
#include <stdbool.h>

#define IV_LEN 12
#define TAG_LEN 16
//#define AAD_LEN 16

void inc_iv(uint8_t *iv);

int hashToBytes(uint8_t *bytesOut, int outLen, const uint8_t *bytesIn, int inLen);
int prf(EVP_CIPHER_CTX *ctx, uint8_t *bytesOut, int outLen, const uint8_t *bytesIn, int inLen);

int symm_encrypt(const uint8_t *key, uint8_t *bytesOut, uint8_t *iv, uint8_t *tag, const uint8_t *bytesIn, int len);
int symm_decrypt(const uint8_t *key, uint8_t *bytesOut, const uint8_t *iv, uint8_t *tag, const uint8_t *bytesIn, int len);

int symm_encrypt(const uint8_t *key, EVP_CIPHER_CTX *ctx, uint8_t *bytesOut, uint8_t *iv, uint8_t *tag, const uint8_t *bytesIn, uint8_t *curr_iv);
int symm_decrypt(const uint8_t *key, EVP_CIPHER_CTX *ctx, uint8_t *bytesOut, const uint8_t *iv, uint8_t *tag, const uint8_t *bytesIn);

void print_bytes(char *label, const uint8_t *bytes, int len);

// Encryption/Decryption with a specific IV.
int symm_encrypt(EVP_CIPHER_CTX *ctx, uint8_t *bytesOut, uint8_t *iv, uint8_t *tag, uint8_t *curr_iv, const uint8_t *bytesIn, int len);
int symm_decrypt(EVP_CIPHER_CTX *ctx, uint8_t *bytesOut, const uint8_t *iv, uint8_t *tag, const uint8_t *bytesIn, int len);

#endif
