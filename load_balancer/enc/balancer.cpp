#include "balancer.h"

//#include <openenclave/enclave.h>
#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <map>

#include "../../common/obl_primitives.h"
#include "../../common/par_obl_primitives.h"
#include "../../common/crypto.h"

using namespace std;

LoadBalancer::LoadBalancer(size_t n, size_t num_blocks, int num_threads) : num_threads(num_threads) {
    num_suborams = n;
    suboram_key = EVP_CIPHER_CTX_new();
    unsigned char *key_bytes = (unsigned char *) "0123456789123456";
    EVP_EncryptInit_ex(suboram_key, EVP_aes_128_ecb(), NULL, key_bytes, NULL);
    EVP_CIPHER_CTX_set_padding(suboram_key, 0);
    client_comm_key.push_back((unsigned char *)"01234567891234567891234567891234");
    
    for (int i = 0; i < num_suborams; i++) {
        suboram_replay_ctr_in.push_back(0);
        suboram_replay_ctr_out.push_back(1);
        suboram_comm_key.push_back((unsigned char *)"01234567891234567891234567891234");
    }

    for (uint32_t i = 1; i < num_blocks; i++) {
        SubOramID sid = get_suboram_for_req(suboram_key, i, num_suborams);
        suboram_key_map[sid % num_suborams].emplace(i);
    }
}

void Batch::add_incoming_request(KeyBlockPairBucketItem req) {
    incoming_reqs.push_back(req);
}

void Batch::add_suboram_response(KeyBlockPairBucketItem resp) {
    suboram_responses.push_back(resp);
}

void LoadBalancer::create_outgoing_batch(Batch &b) {
    /* Assign each request to a subORAM. */
    for (int i = 0; i < b.incoming_reqs.size(); i++) {
        uint32_t tag = !ObliviousEqual(b.incoming_reqs[i].item.block, static_cast<uint8_t *>(nullptr));
        AssignedRequestBucketItem assigned_req(i, b.incoming_reqs[i].item, get_suboram_for_req(suboram_key, b.incoming_reqs[i].item.key, num_suborams), tag);
        b.outgoing_reqs.push_back(assigned_req);
    }

    int index = b.incoming_reqs.size();
    /* Add max number of dummy reqs for each subORAM. */
    for (uint32_t i = 0; i < num_suborams; i++) {
        SubOramID SID = (SubOramID) i;
        set<uint32_t>::iterator it = suboram_key_map[SID].begin();
        int j = 0;
        while (it != suboram_key_map[SID].end() && j < b.reqs_per_suboram) {
            AssignedRequestBucketItem req(index, SID, 0xff);
            req.item.req.key = *it;
            memset(req.item.req.block, 0, BLOCK_LEN);
            b.outgoing_reqs.push_back(req);
            j++;
            it++;
            index++;
            // need to prepopulate this thing
        }
    }

    /* Oblivious sort. */
    //ObliviousSort(b.outgoing_reqs.begin(), b.outgoing_reqs.end(), AssignedRequestBucketItemSorter());
    state.curr_batch = &b;
    notify_workers(WorkerFn::create_outgoing_batch);
    ObliviousSortParallel(b.outgoing_reqs.begin(), b.outgoing_reqs.end(), AssignedRequestBucketItemSorter(), num_threads, 0);
    wait_for_workers();

    /* Tag first padded_reqs_per_suboram reqs for each subORAM. */
    uint32_t prev_SID = -1;     // no match
    uint32_t count = 0;
    uint8_t zero_u8 = 0;
    uint32_t zero_u32 = 0;
    uint8_t one_u8 = 1;
    uint32_t one_u32 = 1;
    uint32_t prev_key = 0xff;
    uint8_t *tags = (uint8_t *)malloc(b.outgoing_reqs.size());
    for (uint32_t i = 0; i < b.outgoing_reqs.size(); i++) {
        bool match = ObliviousEqual(prev_SID, b.outgoing_reqs[i].item.SID);
        uint32_t next_count = count + 1;

        bool can_assign = ObliviousLess(count, b.reqs_per_suboram) && !ObliviousEqual(prev_key, b.outgoing_reqs[i].item.req.key); 
        tags[i] = ObliviousChoose(can_assign, one_u8, zero_u8);
        prev_SID = b.outgoing_reqs[i].item.SID;
        count = ObliviousChoose(match, next_count, zero_u32);
        prev_key = b.outgoing_reqs[i].item.req.key;
    }

    /* Oblivious compaction and truncate. */
    ObliviousCompact(b.outgoing_reqs.begin(), b.outgoing_reqs.end(), tags);
    b.outgoing_reqs.resize(b.reqs_per_suboram * num_suborams);
    free(tags);
}

void LoadBalancer::match_responses_to_clients(Batch &b) {
    /* Merge requests and responses. */
    b.client_responses.reserve(b.suboram_responses.size() + b.incoming_reqs.size());
    b.client_responses.insert(b.client_responses.end(), b.suboram_responses.begin(), b.suboram_responses.end());
    b.client_responses.insert(b.client_responses.end(), b.incoming_reqs.begin(), b.incoming_reqs.end());

    /* Sort by (key, isResp) */
    //ObliviousSort(b.client_responses.begin(), b.client_responses.end(), KeyBlockPairBucketItemSorter());
    notify_workers(WorkerFn::match_responses_to_clients);
    ObliviousSortParallel(b.client_responses.begin(), b.client_responses.end(), KeyBlockPairBucketItemSorter(), num_threads, 0);
    wait_for_workers();

    /* Iterate through and propagate value of block from responses to subsequent requests in list. */
    uint8_t *tags = (uint8_t *)malloc(b.client_responses.size());
    uint8_t zero_u8 = 0;
    uint8_t one_u8 = 1;
    uint8_t curr_block[BLOCK_LEN];
    for (int i = 0; i < b.client_responses.size(); i++) {

        /* Set tag to 0 if isResp so can compact away later. */
        bool isResp = ObliviousEqual(b.client_responses[i].item.isResp, one_u8);
        tags[i] = ObliviousChoose(isResp, zero_u8, one_u8);
    
        /* If isResp, update current block. Otherwise, copy current block. */
        for (int j = 0; j < BLOCK_LEN; j++) {
            curr_block[j] = ObliviousChoose(isResp, b.client_responses[i].item.block[j], curr_block[j]);
            b.client_responses[i].item.block[j] = ObliviousChoose(!isResp, curr_block[j], b.client_responses[i].item.block[j]);
        }
    }

    /* Compact out responses. */
    ObliviousCompact(b.client_responses.begin(), b.client_responses.end(), tags);
    b.client_responses.resize(b.incoming_reqs.size());
    free(tags);
}

void LoadBalancer::worker_loop(int thread_id) {
    int next_iter = 1;
    while (true) {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&, this, next_iter, thread_id] {
            bool ready = state.curr_iter == next_iter;
            /*
            if (ready) {
                printf("[t%d] got job\n", thread_id);
            } else {
                printf("[t%d] waiting for job\n", thread_id);
            }
            */
            return ready;
        });
        lk.unlock();
        if (state.fn == WorkerFn::create_outgoing_batch) {
            ObliviousSortParallel(state.curr_batch->outgoing_reqs.begin(), state.curr_batch->outgoing_reqs.end(), AssignedRequestBucketItemSorter(), num_threads, thread_id);
        } else if (state.fn == WorkerFn::match_responses_to_clients) {
            ObliviousSortParallel(state.curr_batch->client_responses.begin(), state.curr_batch->client_responses.end(), KeyBlockPairBucketItemSorter(), num_threads, thread_id);
        } else if (state.fn == WorkerFn::stop) {
            return;
        }
        next_iter++;
        lk.lock();
        if (++state.n_done == num_threads) {
            lk.unlock();
            cv.notify_all();
        }
    }
}

void LoadBalancer::notify_workers(WorkerFn fn) {
    {
        std::lock_guard<std::mutex> lk(m);
        state.fn = fn;
        state.curr_iter++;
        state.n_done = 1;
    }
    cv.notify_all();
}

void LoadBalancer::wait_for_workers() {
    {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&, this] {
            bool done = state.n_done == num_threads;
            /*
            if (done) {
                printf("all threads done\n");
            } else {
                printf("waiting for threads to finish, done: %d\n", state.n_done);
            }
            */
            return done;
        });
    }
}

void LoadBalancer::stop_workers() {
    notify_workers(WorkerFn::stop);
}

// TODO: cleanup and setup for next batch
