#ifndef _RING_BUFFER_H
#define _RING_BUFFER_H

#include <cstring>
#include <vector>

template <class T>
class RingBuffer {
private:
    std::vector<T> vec;
    size_t head;
    size_t tail;
    size_t capacity;
    size_t mask;

public:
    RingBuffer(size_t num_items) : vec(num_items), head(0), tail(0), capacity(num_items), mask(num_items-1) {}

    RingBuffer() : RingBuffer(2) {}

    void clear() {
        head = 0;
        tail = 0;
    }

    void resize(size_t num_items) {
        clear();
        capacity = num_items;
        mask = num_items - 1;
        vec = std::vector<T>(num_items);
    }

    size_t available_to_read() {
        return tail - head;
    }

    void offset_read(size_t offset, T *data, size_t len) {
        if (offset + len > capacity) {
            size_t fill = capacity - offset;
            memcpy(data, &vec[offset], sizeof(T)*fill);
            size_t rem = len - fill;
            memcpy(data, &vec[0], sizeof(T)*rem);
        } else {
            memcpy(data, &vec[offset], sizeof(T)*len);
        }
    }

    size_t read(T *data, size_t len) {
        size_t avail = this->available_to_read();
        size_t read_items = len;
        if (len > avail) {
            printf("Not reading all, available: %zu\n", avail);
            read_items = avail;
        }
        size_t offset = head & mask;
        offset_read(offset, data, read_items);
        head += read_items;
        return read_items;
    }

    size_t read_full(T *data, size_t len) {
        size_t avail = this->available_to_read();
        if (len > avail) {
            return 0;
        }
        size_t offset = head & mask;
        offset_read(offset, data, len);
        head += len;
        return len;
    }


    size_t available_to_write() {
        return head + mask - tail;
    }

    void offset_write(size_t offset, T *data, size_t len) {
        if (offset + len > capacity) {
            size_t fill = capacity - offset;
            memcpy(&vec[offset], data, sizeof(T)*fill);
            size_t rem = len - fill;
            memcpy(&vec[0], data, sizeof(T)*rem);

        } else {
            memcpy(&vec[offset], data, sizeof(T)*len);
        }
    }

    size_t write(T *data, size_t len) {
        size_t avail = this->available_to_write();
        size_t write_items = len;
        if (len > avail) {
            printf("Not writing all, available: %zu\n", avail);
            write_items = avail;
        }
        size_t offset = tail & mask;
        offset_write(offset, data, write_items);
        tail += write_items;
        return write_items;
    }
};
#endif