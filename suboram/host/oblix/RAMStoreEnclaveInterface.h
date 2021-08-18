#ifndef RAMSTOREENCLAVEINTERFACE_H
#define RAMSTOREENCLAVEINTERFACE_H
#include "RAMStore.hpp"
#include "suboram_u.h"

static RAMStore* store = NULL;

char *ocall_setup_ramStore(size_t num, int size) {
    if (size != -1) {
        store = new RAMStore(num, size, false);
    } else {
        store = new RAMStore(num, size, true);
    }
    return store->buffer.data();
}

void ocall_nwrite_ramStore(size_t blockCount, long long* indexes, size_t len) {
    //printf("[write] blockCount: %d, len: %d\n", blockCount, len);
    assert(len % blockCount == 0);
    size_t eachSize = len / blockCount;
    for (unsigned int i = 0; i < blockCount; i++) {
        block_oblix ciphertext(store->buffer.data() + (i * eachSize), store->buffer.data() + (i + 1) * eachSize);
        store->Write(indexes[i], ciphertext);
    }
}

size_t ocall_nread_ramStore(size_t blockCount, long long* indexes, size_t len) {
    assert(len % blockCount == 0);
    size_t resLen = -1;
    for (unsigned int i = 0; i < blockCount; i++) {
        block_oblix ciphertext = store->Read(indexes[i]);
        resLen = ciphertext.size();
        std::memcpy(&store->buffer[i * resLen], ciphertext.data(), ciphertext.size());
        /*
        printf("[read] index: %d\n", indexes[i]);
        for (auto val : ciphertext) {
            printf("\\x%.2x", val);
        }
        printf("\n");
        printf("[read] (blk) index: %d\n", indexes[i]);
        for (int _i = i*resLen; _i < (i+1)*resLen; _i++) {
            printf("\\x%.2x", store->buffer[_i]);
        }
        printf("\n");
        */
    }
    //printf("[read] blockCount: %d, len: %d, resLen: %d\n", blockCount, len, resLen);
    return resLen;
}

int* testdata = NULL;

void ocall_test_memory_setup(int size) {
    testdata = new int[size];
    for (int i = 0; i < size; i++) {
        testdata[i] = i;
    }
}

void ocall_test_memory_read(int* indexes, int *data) {
    //    for (int i = 0; i < 1; i++) {
    //        int index = indexes[i];
    //        std::memcpy(&data[i], &testdata[index], sizeof(int));
    //    }
    for (int i = 0; i < 300; i++) {
        int index = indexes[i];
        std::memcpy(&data[i], &testdata[index], sizeof (int));
    }
}

void ocall_initialize_ramStore(long long begin, long long end, size_t len) {
    block_oblix ciphertext(store->buffer.begin(), store->buffer.end());
    for (long long i = begin; i < end; i++) {
        store->Write(i, ciphertext);
    }
}

void ocall_write_ramStore(long long index, size_t len) {
    block_oblix ciphertext(store->buffer.data(), &store->buffer[len]);
    store->Write(index, ciphertext);
}
#endif /* RAMSTOREENCLAVEINTERFACE_H */

