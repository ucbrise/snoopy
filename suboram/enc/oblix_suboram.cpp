#include "suboram.h"

#include "oblix/ORAM.hpp"
#include "oblix/OMAP.h"
#include "oblix/Bid.h"

void uint_to_oblixpp_id(unsigned int id, byte_t *arr) {
    for (int i = 0; i < ID_SIZE; i++) {
        if (id == 0) {
            break;
        }
        arr[i] = (byte_t) id & 0xff;
        id = id >> 8;
    }
}

int SuboramDispatcher::local_oblix_init() {
    printf("Oblix init\n");
    std::array<byte_t,128> tmp_key{0};
    OMAP *omap = new OMAP(num_local_blocks, tmp_key);
    printf("Finished OMAP constructor\n");

    uint8_t *block_key = (uint8_t *) "0123456789123456"; // TODO: Change keys
    EVP_CIPHER_CTX *suboram_hash_key = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(suboram_hash_key, EVP_aes_128_ecb(), NULL, block_key, NULL);
    
    map<Bid, string> pairs;
    uint8_t empty_block[16];
    //uint8_t empty_block[BLOCK_LEN];
    memset(empty_block, 0, 16);
    //memset(empty_block, 0, BLOCK_LEN);
    //std::string empty_block_str((char *)empty_block, BLOCK_LEN);
    std::string empty_block_str((char *)empty_block, 16);
    for (unsigned long long i = 1; i <= num_total_blocks; i++) {
        if ((get_suboram_for_req(suboram_hash_key, i, num_suborams) == suboram_id)) {
            //byte_t id[ID_SIZE] = { 0 };
            //uint_to_oblixpp_id(i, id);
            Bid id(i);
            pairs[id] = empty_block_str;
            omap->insert(id, empty_block_str);
        }
    }
    printf("going to batch insert\n");
    //omap->batchInsert(pairs);
    printf("Finished OMAP batch insert\n");

    free(suboram_hash_key);
    printf("Finished oblix init\n");
    
    int rv = init(NULL);
    printf("Did dispatcher init\n");
    return rv;
}
