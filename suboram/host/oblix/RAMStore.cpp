#include "RAMStore.hpp"

RAMStore::RAMStore(size_t count, size_t ram_size, bool simul)
: store(count), size(ram_size), buffer(10000*ram_size) {
    this->simulation = simul;
}

RAMStore::~RAMStore() {
}

block_oblix RAMStore::Read(long long pos) {
    if (simulation) {
        return store[0];
    } else {
        return store[pos];
    }
}

void RAMStore::Write(long long pos, block_oblix b) {
    if (!simulation) {
        store[pos] = b;
    } else {
        store[0] = b;
    }
}
