// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.
#include "host.h"

#include <stdio.h>
#include <assert.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <math.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <fstream>
//#include "suboram_store.h"
#include "../../common/bucket_sort.h"
#include "../../common/common.h"
#include "../../common/obl_primitives.h"
#include "../../build/common/oram.grpc.pb.h"
#include "../enc/suboram.h"
// oblix
#include "oblix/RAMStoreEnclaveInterface.h"



#define BENCH_EXTERNAL_SORT false

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using oram::Oram;
using oram::OramRequests;
using oram::OramRequest;
using oram::OramResponses;
using oram::OramResponse;

using namespace std;

void bench_enclave_bitonic_sort(oe_enclave_t *enclave, int *ret, int num_trials, int num_blocks);
void bench_enclave_bucket_sort(oe_enclave_t *enclave, int *ret, int num_trials, int num_blocks);

mutex batch_lock;

class SuboramServiceImpl final : public Oram::Service {
public:
    SuboramHost &host;

    SuboramServiceImpl(SuboramHost &host) : host(host) {}

    Status BatchedAccessKeys(ServerContext *context, const OramRequests *req,
            OramResponses *resp) override {
        // TODO: actually handle the responses and send back a response for each request
        uint32_t num_reqs = req->len();
        uint8_t *in_ct = (uint8_t *)malloc(get_key_val_buf_sz(num_reqs));
        uint8_t in_iv[IV_LEN];
        uint8_t in_tag[TAG_LEN];
        uint8_t *out_ct = (uint8_t *)malloc(get_key_val_buf_sz(num_reqs));
        uint8_t out_iv[IV_LEN];
        uint8_t out_tag[TAG_LEN];

        memcpy(in_ct, req->ct().c_str(), get_key_val_buf_sz(num_reqs));
        memcpy(in_iv, req->iv().c_str(), IV_LEN);
        memcpy(in_tag, req->tag().c_str(), TAG_LEN);

        int32_t balancer_id = req->balancer_id();
        batch_lock.lock();

        std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
        host.run_process_batch(in_ct, in_iv, in_tag, out_ct, out_iv, out_tag, num_reqs, req->balancer_id());
        std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
        cout << "process batch [" << num_reqs << "] total: " << std::chrono::duration_cast<std::chrono::microseconds>(t2-t1).count() << endl;

        batch_lock.unlock();

        resp->set_ct(out_ct, get_key_val_buf_sz(num_reqs));
        resp->set_iv(out_iv, IV_LEN);
        resp->set_tag(out_tag, TAG_LEN);
        resp->set_len(num_reqs);

        free(in_ct);
        free(out_ct);

        return Status::OK;
    }
};

void runServer(string addr, SuboramHost &host) {
    SuboramServiceImpl service(host);

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    unique_ptr<Server> server(builder.BuildAndStart());
    printf("Started suboram server\n");
    fflush(stdout);
    server->Wait();
}

int main(int argc, const char* argv[]) {
    oe_enclave_t *enclave = NULL;
    oe_result_t result = OE_OK;
    char* server_port = NULL;
    int block_sz; 
    int actual_block_sz;
    bool keep_server_up = false;
    block_ct *arr = NULL;
    //SuboramHost *host;

    ifstream config_stream(argv[2]);
    json config;
    config_stream >> config;

    SuboramHost host(config[NUM_BLOCKS], config[NUM_BALANCERS], config[NUM_SUBORAMS], config[SUBORAM_ID], config[PROTOCOL_TYPE], config[SORT_TYPE], config[MODE], config[THREADS], config[OBLIX_BASELINE][OBLIX_BASELINE_160]);

    //SuboramHost host(config[NUM_BLOCKS], config[NUM_BALANCERS], config[PROTOCOL]);
    printf("Host: Creating an tls client enclave\n");
    // NEED TO COMMENT THIS BACK IN
    enclave = host.create_enclave(argv[1]);
    if (enclave == NULL)
    {
        goto exit;
    }

    // TODO: also assert power of 2
   
    host.init_enclave();
    //host.mock_init_enclave();
    /*
    printf("** starting suboram server\n");
    runServer("0.0.0.0:" + string(config[LISTENING_PORT]), host);    // blocking
    */
    //host.bench_scan_blocks(1);
    //host.bench_process_batch(1);
    {
    chrono::steady_clock::time_point begin = chrono::steady_clock::now();
    if (config[MODE] == BENCH_SORT) {
        host.bench_sort(100);
    } else if (config[MODE] == BENCH_PROCESS_BATCH) {
        host.bench_process_batch(10);
    } else if (config[MODE] == SERVER) {
        runServer("0.0.0.0:12346", host);    // blocking
    } else {
        throw std::runtime_error("Unsupported suboram mode: " + std::string(config[MODE]));
    }
    

    // TODO: also assert power of 2
    /*
    for (int i = 0; i < num_blocks / max_enc_blocks; i++) {
        init_arr(enclave, &ret, (char *)&arr[i * max_enc_blocks], i * max_enc_blocks, max_enc_blocks);
    }
    */
    //printf("** starting suboram server\n");
    //runServer("0.0.0.0:12346", host);    // blocking
    // TODO: need to actually sort based on permutation
    //host.bench_buffered_bucket_sort(1);
    //host.bench_mock_enclave_buffered_bucket_sort(1);
    /*for (int i = 0; i < num_blocks / max_enc_blocks; i++) {
        check_arr(enclave, (char *)&arr[i * max_enc_blocks], max_enc_blocks);
    }*/

    chrono::steady_clock::time_point end = chrono::steady_clock::now();
    printf("Total enclave time: %ld\n", std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());
    }
exit:
    printf("Host: Terminating enclaves\n");
    if (enclave)
        host.terminate_enclave();
    printf("Host:  %s \n", (host.ret == OKAY) ? "succeeded" : "failed");
    if (host.ret != OKAY) printf("******* ERROR ********\n");
    return host.ret;
}

oe_enclave_t* SuboramHost::create_enclave(const char* enclave_path) {
    enclave = NULL;

    printf("Host: Enclave library %s\n", enclave_path);
    oe_result_t result = oe_create_suboram_enclave(
        enclave_path,
        OE_ENCLAVE_TYPE_SGX,
        OE_ENCLAVE_FLAG_DEBUG
#ifdef OE_SIMULATION
            | OE_ENCLAVE_FLAG_SIMULATE
#endif
            ,
        NULL,
        0,
        &enclave);

    if (result != OE_OK)
    {
        printf(
            "Host: oe_create_remoteattestation_enclave failed. %s",
            oe_result_str(result));
    }
    else
    {
        printf("Host: Enclave successfully created.\n");
    }
}

void SuboramHost::terminate_enclave()
{
    oe_terminate_enclave(enclave);
    printf("Host: Enclave successfully terminated.\n");
}

void SuboramHost::init_enclave() {
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    if (protocol == OUR_PROTOCOL) {
        set_params(enclave, &ret, num_blocks, blocks_per_bucket, num_balancers, num_suborams, suboram_id, &num_local_blocks, num_threads);
        if (mode == BENCH_SORT) {
            init_bench_sort(enclave, &ret, &sorter.s);
        } else if (mode == SERVER || mode == BENCH_PROCESS_BATCH) {
            num_buckets = num_local_blocks / blocks_per_bucket;
            block_ct = std::vector<BucketCT<block>>(num_buckets);
            for (int i = 0; i < num_buckets; i++) {
                block_ct[i].init(blocks_per_bucket);
            }
            std::thread host_thread([this] () mutable {
                pin_host_thread();
                fill_block_ct();
            });
            std::thread enclave_thread = spawn_enclave_thread(init, enclave, &ret, &s);
            enclave_thread.join();
            host_thread.join();
        }
    } else if (protocol == OBLIX_PROTOCOL) {
        set_params(enclave, &ret, num_blocks, blocks_per_bucket, num_balancers, num_suborams, suboram_id, &num_local_blocks, num_threads);
        printf("returned from oblix init in host\n");
        int levels = calc_oblix(num_local_blocks, 0, 512*8, 0);
        printf("Access time: %.6f Levels: %d\n", oblix_access_time, levels);
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    printf("Init time for %d total blocks: %f us\n", num_blocks, ((double)std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()));
}

void SuboramHost::mock_init_enclave() {
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    mock_suboram.set_params(num_blocks, blocks_per_bucket, num_balancers, num_suborams, suboram_id, num_threads);
    num_local_blocks = mock_suboram.num_local_blocks;
    if (mode == BENCH_PROCESS_BATCH) {
        num_buckets = num_local_blocks / blocks_per_bucket;
        block_ct = std::vector<BucketCT<block>>(num_buckets);
        for (int i = 0; i < num_buckets; i++) {
            block_ct[i].init(blocks_per_bucket);
        }
        std::thread host_thread([this] () mutable {
            pin_host_thread();
            fill_block_ct();
        });
        std::thread enclave_thread( [&, this] {
            mock_suboram.init(&s);
        });
        enclave_thread.join();
        host_thread.join();
    } else {
        throw std::runtime_error("Unimplemented mock suboram mode: " + std::string(mode));
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    printf("Init time for %d total blocks: %f us\n", num_blocks, ((double)std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()));
}


void SuboramHost::run_buffered_bucket_sort() {
    std::thread host_thread([this] () mutable {
        //pin_host_thread();
        sorter.fill_buffer();
    });
    std::vector<std::thread> enclave_threads;
    for (int i = 0; i < num_threads; i++) {
        enclave_threads.push_back(spawn_enclave_thread_on_cpu(i, buffered_bucket_sort, enclave, &ret, i));
    }
    for (int i = 0; i < num_threads; i++) {
        enclave_threads[i].join();
    }
    host_thread.join();
}

void SuboramHost::run_parallel_bitonic_sort() {
    std::vector<std::thread> enclave_threads;
    for (int i = 1; i < num_threads; i++) {
        enclave_threads.push_back(spawn_enclave_thread_on_cpu(i+1, parallel_bitonic_sort, enclave, &ret, i));
    }
    parallel_bitonic_sort(enclave, &ret, 0);
    for (int i = 1; i < num_threads; i++) {
        enclave_threads[i-1].join();
    }
}

void SuboramHost::run_parallel_bitonic_sort_nonadaptive() {
    std::vector<std::thread> enclave_threads;
    for (int i = 1; i < num_threads; i++) {
        enclave_threads.push_back(spawn_enclave_thread_on_cpu(i+1, parallel_bitonic_sort_nonadaptive, enclave, &ret, i));
    }
    parallel_bitonic_sort_nonadaptive(enclave, &ret, 0);
    for (int i = 1; i < num_threads; i++) {
        enclave_threads[i-1].join();
    }
}

void SuboramHost::run_shuffle() {
    std::thread host_thread([this] () mutable {
        pin_host_thread();
        sorter.fill_buffer();
    });
    std::thread enclave_thread = spawn_enclave_thread(shuffle_blocks, enclave, &ret);
    enclave_thread.join();
    host_thread.join();
}

void SuboramHost::run_mock_buffered_bucket_sort() {
    num_local_blocks = num_blocks;
    std::vector<block_bucket_item> blocks(num_blocks);
    sorter.mock_enclave_sort(blocks, cmp_block_pos, num_threads);
}

void SuboramHost::run_process_batch(uint8_t *in_ct, uint8_t *in_iv, uint8_t *in_tag, uint8_t *out_ct, uint8_t *out_iv, uint8_t *out_tag, int batch_sz, uint32_t balancer_id) {
    if (protocol == OUR_PROTOCOL) {
        std::thread host_thread([this] {
            fill_block_ct();
        });
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; i++) {
            threads.push_back(spawn_enclave_thread_on_cpu(i, table_process_batch_parallel, enclave, &ret, in_ct, in_iv, in_tag, out_ct, out_iv, out_tag, batch_sz, balancer_id, i));
        }
        for (auto &thread : threads) {
            thread.join();
        }
        host_thread.join();
    } else if (protocol == OBLIX_PROTOCOL) {
        chrono::steady_clock::time_point begin = chrono::steady_clock::now();
        double total_time = batch_sz * oblix_access_time * 1e3;
        std::thread enclave_thread = spawn_enclave_thread(oblix_process_batch, enclave, &ret, in_ct, in_iv, in_tag, out_ct, out_iv, out_tag, batch_sz, balancer_id);
        enclave_thread.join();
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        double time_elapsed = (double) std::chrono::duration_cast<std::chrono::milliseconds>(end-begin).count();
        while (time_elapsed < total_time) {
            std::this_thread::sleep_for(std::chrono::milliseconds((int) ceil(total_time - time_elapsed)));
            end = std::chrono::steady_clock::now();
            time_elapsed = (double) std::chrono::duration_cast<std::chrono::milliseconds>(end-begin).count();
        }
    }
}

void SuboramHost::run_mock_process_batch(uint8_t *in_ct, uint8_t *in_iv, uint8_t *in_tag, uint8_t *out_ct, uint8_t *out_iv, uint8_t *out_tag, int batch_sz, uint32_t balancer_id) {
    if (protocol == OUR_PROTOCOL) {
        std::thread host_thread([this] {
            fill_block_ct();
        });
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; i++) {
            threads.push_back(std::thread([&, this, i] {
                mock_suboram.ecall_process_requests_parallel(&ret, in_ct, in_iv, in_tag, out_ct, out_iv, out_tag, batch_sz, balancer_id, i);
            }));
        }
        for (auto &thread : threads) {
            thread.join();
        }
        host_thread.join();
    } else {
        throw std::runtime_error("Unimplemented run_mock_process_batch protocol: " + std::string(protocol));
    }
}

void SuboramHost::bench_process_batch(int num_trials) {
    uint32_t replay_counter = 1;
    //for (int i = 1; i < 150; i++) {
    for (int i = 6; i < 14; i++) {
    //for (int i = 6; i < 15; i++) {
        double total_time = 0;
        //int batch_sz = i * 1000;
        int batch_sz = 1 << i;
        if (batch_sz > num_blocks) {
            break;
        }
        for (int j = 0; j < num_trials; j++) {
            uint32_t *keys = (uint32_t *) malloc(sizeof(uint32_t)*batch_sz);
            uint8_t **in_arr = (uint8_t **) malloc(sizeof(uint8_t *)*batch_sz);
            for (int i = 0; i < batch_sz; i++) {
                keys[i] = i;
                in_arr[i] = (uint8_t *) malloc(BLOCK_LEN);
            }
            uint8_t *in_ct = (uint8_t *)malloc(get_key_val_buf_sz(batch_sz));
            uint8_t in_iv[IV_LEN];
            uint8_t in_tag[TAG_LEN];
            uint8_t *out_ct = (uint8_t *)malloc(get_key_val_buf_sz(batch_sz));
            uint8_t out_iv[IV_LEN];
            uint8_t out_tag[TAG_LEN];
            uint8_t *key = (uint8_t *) "01234567891234567891234567891234";
            encrypt_key_val_pairs(key, in_ct, in_iv, in_tag, keys, in_arr, batch_sz, &replay_counter, true);
            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
            run_process_batch(in_ct, in_iv, in_tag, out_ct, out_iv, out_tag, batch_sz, 0);
            //run_mock_process_batch(in_ct, in_iv, in_tag, out_ct, out_iv, out_tag, batch_sz, 0);
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            total_time += std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
        }
        printf("Process batch time for %d total blocks, %d batch size: %f us\n", num_blocks, batch_sz, total_time / ((double)num_trials));
    }
}

void SuboramHost::fill_block_ct() {
    for (int i = 0; i < block_ct.size(); i+=BLOCK_BUF_BUCKETS) {
        s.host_q.write(&block_ct[i], BLOCK_BUF_BUCKETS);
        fetch_from_queue();
        memcpy(&block_ct[i], buf, BLOCK_BUF_BUCKETS*sizeof(BucketCT<block>));
    }
}

void SuboramHost::fetch_from_queue() {
    volatile int read = 0;
    int i = 0;
    while (read <= 0) {
        read = s.sgx_q.read_full(buf, BLOCK_BUF_BUCKETS);
        i++;
    }
}

void SuboramHost::bench_scan_blocks(int num_trials) {
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    for (int i = 0; i < num_trials; i++) {
        std::thread host_thread([this] () mutable {
            pin_host_thread();
            fill_block_ct();
        });
        std::thread enclave_thread = spawn_enclave_thread(scan_blocks, enclave, &ret);
        enclave_thread.join();
        host_thread.join();
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    printf("Scan time for %d total blocks: %f us\n", num_blocks, ((double)std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()) / ((double)num_trials));
}

void SuboramHost::bench_mock_enclave_buffered_bucket_sort(int num_trials) {
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    for (int i = 0; i < num_trials; i++) {
        run_mock_buffered_bucket_sort();
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    printf("Sort time for %d total blocks: %f us\n", sorter.s.num_blocks, ((double)std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()) / ((double)num_trials));
}

void SuboramHost::bench_sort(int num_trials) {
    verify_sorted(enclave, &ret);
    printf("Sorted: %d\n", ret);
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    for (int i = 0; i < num_trials; i++) {
        if (sort_type == BUFFERED_BUCKET_SORT) {
            run_buffered_bucket_sort();
            //run_mock_buffered_bucket_sort();
        } else if (sort_type == BUCKET_SORT) {
            bucket_sort(enclave, &ret);
        } else if (sort_type == BITONIC_SORT) {
            run_parallel_bitonic_sort();
        } else if (sort_type == BITONIC_SORT_NONADAPTIVE) {
            run_parallel_bitonic_sort_nonadaptive();
        } else {
            throw std::runtime_error("Unsupported sort type: " + sort_type);
        }
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    printf("(%d threads) %s time for %d total blocks: %f us\n",
        num_threads, sort_type.c_str(), num_local_blocks,
        ((double)std::chrono::duration_cast<std::chrono::microseconds>(end -
        begin).count()) / ((double)num_trials));
    verify_sorted(enclave, &ret);
    printf("Sorted: %d\n", ret);
}

void bench_enclave_bitonic_sort(oe_enclave_t *enclave, int *ret, int num_trials, int num_blocks) {
    printf("starting bench");
    verify_sorted(enclave, ret);
    printf("Sorted: %d\n", *ret);
    chrono::steady_clock::time_point begin = chrono::steady_clock::now();
    for (int i = 0; i < num_trials; i++) {
        bitonic_sort(enclave, ret);
        //insecure_sort(enclave, ret);
    }
    chrono::steady_clock::time_point end = chrono::steady_clock::now();
    printf("Sort time for %d total blocks: %f us\n", num_blocks, ((double)std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()) / ((double)num_trials));
    printf("Sorted: %d\n", *ret);
    verify_sorted(enclave, ret);
}

void prefetcher(oe_enclave_t *enclave) {
    prefetch_bucket_sort(enclave);
}

double SuboramHost::get_time_for_b(int b) {
    if (b <= 10) {
        return oblix_baseline[OBLIX_BASELINE_10];
    } else if (b <= 100) {
        return oblix_baseline[OBLIX_BASELINE_100];
    } else if (b <= 1000) {
        return oblix_baseline[OBLIX_BASELINE_1000];
    } else if (b <= 10000) {
        return oblix_baseline[OBLIX_BASELINE_10000];
    } else if (b <= 100000) {
        return oblix_baseline[OBLIX_BASELINE_100000];
    } else if (b <= 1000000) {
        return oblix_baseline[OBLIX_BASELINE_1000000];
    }
}

int SuboramHost::calc_oblix(int n, int ctr, int b, double access_time) {
   ctr += 1;

   //n is no. of blocks
   int buckets = (int) ceil(n / 5.0);
   int levels = (int) ceil(log(buckets));
   int leaves = pow(2, levels - 1);

   int o = (int) ceil((double)(n*levels)/(double)b);
   oblix_access_time = access_time;
   if (o > 1) {
       return calc_oblix(o, ctr, b, access_time+get_time_for_b(o));
   } else {
       return ctr;
   }
}

/*
void bench_enclave_bucket_sort(oe_enclave_t *enclave, int *ret, int num_trials, int num_blocks) {
    chrono::steady_clock::time_point begin = chrono::steady_clock::now();
    init(enclave, ret, num_blocks, 1, &(void *) sorter.s);
    chrono::steady_clock::time_point end = chrono::steady_clock::now();
    printf("Init time for %d total blocks: %f us\n", num_blocks, ((double)std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()) / ((double)num_trials));
    printf("starting bench\n");
    //verify_sorted(enclave, ret);
    //printf("Sorted: %d\n", *ret);
    begin = chrono::steady_clock::now();
    for (int i = 0; i < num_trials; i++) {
        //scan_blocks(enclave, ret);
        melbourne_sort(enclave, ret);
        //std::thread t(prefetcher, enclave);
        //bucket_sort(enclave, ret);
        //t.join();
        //insecure_sort(enclave, ret);
    }
    end = chrono::steady_clock::now();
    printf("Sort time for %d total blocks: %f us\n", num_blocks, ((double)std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()) / ((double)num_trials));
    //verify_sorted(enclave, ret);
    //printf("Sorted: %d\n", *ret);
}
*/
// TODO: if want to be able to run this test, need to pass in ciphertexts to
// enclave
// TODO: make this test more robust, basically just making sure it doesn't segfault right now
/*void test_process_batch(oe_enclave_t *enclave, int *ret, int num_reqs) {
    uint32_t *key_arr = (uint32_t *)malloc(num_reqs * sizeof(uint32_t));
    uint8_t **in_data_arr = (uint8_t **)malloc(num_reqs * sizeof(uint8_t *));
    uint8_t **out_data_arr = (uint8_t **)malloc(num_reqs * sizeof(uint8_t *));
    for (int i = 0; i < num_reqs; i++) {
        in_data_arr[i] = (uint8_t *)malloc(BLOCK_LEN);
        out_data_arr[i] = (uint8_t *)malloc(BLOCK_LEN);
    }
    chrono::steady_clock::time_point t1 = chrono::steady_clock::now();
    process_batch(enclave, ret, key_arr, in_data_arr, out_data_arr, num_reqs);
    chrono::steady_clock::time_point t2 = chrono::steady_clock::now();
    printf("Time to process batch for %d reqs: %f us\n", num_reqs, ((double)std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count()));
    printf("Finished processing batch\n");
}*/
