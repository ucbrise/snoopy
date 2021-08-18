#include "bucket_sort.h"

namespace bs {
    std::mutex m;
    std::condition_variable cv;
    thread_state state;
}

BucketSortParams::BucketSortParams() : num_blocks(1), log_num_blocks(1), z(1), log_total_buckets(1), total_buckets(1), buf_size(1) {}

BucketSortParams::BucketSortParams(int n) : num_blocks(n) {
    num_blocks = std::max(num_blocks, 1);
    log_num_blocks = log2(num_blocks);
    z = get_z(num_blocks);
    z = 1 << (int) ceil(log2(z));
    //z = std::min(num_blocks, 65536);
    log_total_buckets = ceil(log2(num_blocks * 2 / z));
    total_buckets = 1 << log_total_buckets;
    int log_buf_size = ceil(log2(16 * z + 1));
    buf_size = 1 << log_buf_size;
    //buf_size = 1;
    printf("num blocks: %d, log_total_buckets: %d, z: %d, buf size: %zu\n", n, log_total_buckets, z, buf_size);
}

int BucketSortParams::get_z(int n) {
    if (n <= 1 << 8) {
        return 52;
    } else if (n <= 1 << 9) {
        return 61;
    } else if (n <= 1 << 10) {
        return 70;
    } else if (n <= 1 << 11) {
        return 78;
    } else if (n <= 1 << 12) {
        return 87;
    } else if (n <= 1 << 13) {
        return 95;
    } else if (n <= 1 << 14) {
        return 103;
    } else if (n <= 1 << 15) {
        return 112;
    } else if (n <= 1 << 16) {
        return 120;
    } else if (n <= 1 << 17) {
        return 129;
    } else if (n <= 1 << 18) {
        return 145;
    } else if (n <= 1 << 20) {
        return 154;
    } else if (n <= 1 << 21) {
        return 162;
    } else if (n <= 1 << 22) {
        return 171;
    } else if (n <= 1 << 23) {
        return 179;
    } else {
        return -1;
    }
}

void pin_host_thread() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    int rc = sched_setaffinity(0, sizeof(cpuset), &cpuset);
    if (rc != 0) {
        std::cerr << "Error calling sched_setaffinity: " << rc << "\n";
    }
}