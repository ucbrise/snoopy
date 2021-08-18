#pragma once
#include <map>
#include <array>
#include <vector>

using namespace std;

using byte_t = uint8_t;
using block_oblix = std::vector<byte_t>;

class RAMStore {
    std::vector<block_oblix> store;
    size_t size;
    bool simulation;


public:
    std::vector<char> buffer;

    RAMStore(size_t num, size_t size, bool simulation);
    ~RAMStore();

    block_oblix Read(long long pos);
    void Write(long long pos, block_oblix b);

};
