#ifndef _BALANCER_
#define _BALANCER_

#include <stdlib.h>
#include <string.h>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <condition_variable>
#include <openssl/evp.h>

#include "../../common/block.h"
#include "../../common/bucket_sort.h"

using namespace std;
using namespace lb_types;

class Batch {
    public:
        vector<KeyBlockPairBucketItem> incoming_reqs;
        vector<AssignedRequestBucketItem> outgoing_reqs;
        vector<KeyBlockPairBucketItem> suboram_responses;
        vector<KeyBlockPairBucketItem> client_responses;
        uint32_t reqs_per_suboram;
        EnclaveBucketSorter<AssignedRequestBucketItem> outgoing_req_sorter;
        EnclaveBucketSorter<KeyBlockPairBucketItem> client_resp_sorter;


       Batch(uint32_t reqs_per_suboram) : reqs_per_suboram(reqs_per_suboram) {}
       void add_incoming_request(KeyBlockPairBucketItem req);
       void add_suboram_response(KeyBlockPairBucketItem req);

};

class LoadBalancer {
    public:
        size_t num_suborams;
        vector<unsigned char *>client_comm_key;
        vector<unsigned char *>suboram_comm_key;
        map<uint32_t, uint32_t>client_replay_ctr_in;
        vector<uint32_t>suboram_replay_ctr_in;
        map<uint32_t, uint32_t>client_replay_ctr_out;
        vector<uint32_t>suboram_replay_ctr_out;
        map<SubOramID, set<uint32_t>> suboram_key_map;
        int num_threads;

        LoadBalancer(size_t n, size_t num_blocks, int num_threads = 1);
        void create_outgoing_batch(Batch &b);
        void match_responses_to_clients(Batch &b);
        void worker_loop(int thread_id);
        void stop_workers();

    private:
        enum class WorkerFn{
            create_outgoing_batch,
            match_responses_to_clients,
            stop,
        };
        struct worker_state {
            WorkerFn fn;
            Batch *curr_batch;
            int n_done;
            int curr_iter = 0;
        };
        std::mutex m;
        std::condition_variable cv;
        worker_state state;

        void notify_workers(WorkerFn fn);
        void wait_for_workers();

        EVP_CIPHER_CTX *suboram_key;
};

#endif
