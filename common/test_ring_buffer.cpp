#include <cassert>
#include <iostream>

#include "ring_buffer.h"
#include "block.h"

using Block = bucket_item<block>;

int main() {
    size_t TEST_NUM_BLOCKS = 63;
    RingBuffer<Block> rbuf(TEST_NUM_BLOCKS+1);

    Block b;
    b.index = 1;
    rbuf.write(&b, 1);
    assert_eq(rbuf.available_to_write(), TEST_NUM_BLOCKS-1);
    assert_eq(rbuf.available_to_read(), 1);
    Block rb;
    rbuf.read(&rb, 1);
    assert_eq(rb.index, 1);
    assert_eq(rbuf.available_to_write(), TEST_NUM_BLOCKS);
    assert_eq(rbuf.available_to_read(), 0);

    std::vector<Block> bvec;
    for (int i = 0; i < TEST_NUM_BLOCKS; i++) {
        Block b;
        b.index = i;
        bvec.push_back(b);
    }
    rbuf.write(bvec.data(), TEST_NUM_BLOCKS);
    std::vector<Block> buf(10);
    int batch_size = 4;
    int offset = 0;
    for (int i = 0; i < TEST_NUM_BLOCKS / batch_size; i++) {
        assert_eq(rbuf.available_to_write(), i*batch_size);
        assert_eq(rbuf.available_to_read(), TEST_NUM_BLOCKS-i*batch_size);
        assert_eq(rbuf.read(buf.data(), batch_size), batch_size);
        for (int j = 0; j < batch_size; j++) {
            assert_eq(buf[j].index, i*batch_size+j);
        }
        offset += batch_size;
    }
    int rem = TEST_NUM_BLOCKS % batch_size;
    assert_eq(rbuf.available_to_write(), TEST_NUM_BLOCKS-rem);
    assert_eq(rbuf.available_to_read(), rem);
    assert_eq(rbuf.read_full(buf.data(), batch_size), 0);
    assert_eq(rbuf.read(buf.data(), rem), rem);
    for (int j = 0; j < rem; j++) {
        assert_eq(buf[j].index, offset+j);
    }
    
    assert_eq(rbuf.available_to_write(), TEST_NUM_BLOCKS);
    assert_eq(rbuf.available_to_read(), 0);
}