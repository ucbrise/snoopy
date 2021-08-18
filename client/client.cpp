// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifdef _WIN32
#include <ws2tcpip.h>
#define close closesocket
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <string.h>
#include <fstream>
#include <vector>
#include <thread>
#include <stdlib.h>

#include <openenclave/host.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

#include <grpcpp/grpcpp.h>

//#include <boost/thread.hpp>

#include "../common/common.h"
#include "../common/block.h"
#include "../common/json.hpp"

#include "../build/common/oram.grpc.pb.h"

#include "log.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;
using grpc::Status;
using oram::OramRequest;
using oram::OramResponse;
using oram::Oram;

using json = nlohmann::json;

using namespace std;

/*
typedef boost::shared_mutex Lock;
typedef boost::unique_lock<Lock> UniqueLock;
typedef boost::shared_lock<Lock> SharedLock;
*/

class OramClient {
    public:
        OramClient(std::shared_ptr<Channel> channel, uint32_t id)
            : stub_(Oram::NewStub(channel)) {
            comm_key_ = (unsigned char *) "01234567891234567891234567891234";
            replay_ctr_in_ = 0;
            replay_ctr_out_ = 1;  
            id_ = id;
        }

        int MakeReq(uint32_t key, uint8_t *in_data, uint8_t *out_data, int batchSz, int numBlocks) {
            OramRequest req;
            int ct_len = get_key_val_buf_sz();
            uint8_t *buf = (uint8_t *)malloc(ct_len);
            uint8_t iv[IV_LEN];
            uint8_t tag[TAG_LEN];
            for (int i = 0; i < batchSz; i++) {
                int reqKey = rand() % (numBlocks - 1) + 1;
                int isRead = (rand() % 2) == 0;
                if (isRead) {
                    encrypt_read_key(comm_key_, buf, iv, tag, reqKey, &replay_ctr_out_, false);
                } else {
                    encrypt_key_val(comm_key_, buf, iv, tag, reqKey, in_data, &replay_ctr_out_, false);
                }
                req.add_ct(buf, ct_len);
                req.add_iv(iv, IV_LEN);
                req.add_tag(tag, TAG_LEN);
            }
            replay_ctr_out_++;
            req.set_client_id(id_);
            OramResponse resp;
            ClientContext ctx;
            Status status;

            //printf("[C] Sending request for key %d\n", key);
            std::unique_ptr<ClientAsyncResponseReader<OramResponse>> rpc(stub_->AsyncAccessKey(&ctx, req, &cq_));
            rpc->Finish(&resp, &status, (void *) 1);
            void *got_tag;
            bool ok = false;
            cq_.Next(&got_tag, &ok);
            if (ok && got_tag == (void *) 1) {
                if (status.ok()) {
                    uint8_t tag[TAG_LEN];
                    //memcpy(tag, resp.tag(0).c_str(), TAG_LEN);
                    for (int i = 0; i < batchSz; i++) {
                        memcpy(tag, resp.tag(i).c_str(), TAG_LEN);
                        decrypt_key_val(comm_key_, &key, out_data, (const uint8_t *)resp.ct(i).c_str(), (const uint8_t *)resp.iv(i).c_str(), tag, &replay_ctr_in_, false);
                    }
                    replay_ctr_in_++;
                    //printf("[C] Received answer for key %d\n", key);
                    //print_bytes("Client receiving data", out_data, BLOCK_LEN);
                    return OKAY;
                } else {
                    printf("[C] ERROR sending req");
                    printf(" (error code: %d, error message: %s)\n", status.error_code(), status.error_message().c_str());
                }
            }
            return ERROR;
        }
    private:
        std::unique_ptr<Oram::Stub> stub_;
        CompletionQueue cq_;
        unsigned char *comm_key_;
        uint32_t replay_ctr_in_;
        uint32_t replay_ctr_out_;
        uint32_t id_;
};

void requestThread(string target_str, int id, uint32_t secToRun, vector<vector<uint32_t>> *msLatencyLists, int batchSz, int numBlocks, chrono::steady_clock::time_point *begin_exp) {
    uint8_t block_bytes[BLOCK_LEN];
    OramClient *client = new OramClient(grpc::CreateChannel(
                target_str, grpc::InsecureChannelCredentials()), id);
    memset(block_bytes, 0xff, BLOCK_LEN);
   
    // TODO: locking for begin_exp
    //SharedLock r_lock(*lock);
    while (*begin_exp == chrono::steady_clock::time_point::max() || chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - *begin_exp).count() < secToRun) {
        //r_lock.unlock();
        chrono::steady_clock::time_point begin = chrono::steady_clock::now();
        client->MakeReq(id, block_bytes, block_bytes, batchSz, numBlocks);
        chrono::steady_clock::time_point end = chrono::steady_clock::now();
        //r_lock.lock();
        if (*begin_exp != chrono::steady_clock::time_point::max() && chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - *begin_exp).count() < secToRun) {
            (*msLatencyLists)[id].push_back(chrono::duration_cast<chrono::milliseconds>(end - begin).count());
        }
    }
    //r_lock.unlock();
}

int main(int argc, char** argv)
{
    debug_log::set_name("client");
    int ret = 1;
    X509* cert = nullptr;
    SSL_CTX* ctx = nullptr;
    SSL* ssl = nullptr;
    int serversocket = 0;
    int error = 0;
    uint8_t block_bytes[BLOCK_LEN];
    //std::string target_str = "localhost:12345";
    OramClient *client;
    ifstream config_stream(argv[1]);
    json config;
    config_stream >> config;
    vector<thread *>threads;
    string experiment_dir = config[EXP_DIR];
    int num_balancers = config[NUM_BALANCERS];
    //ofstream file(experiment_dir + "/results.dat");
    ofstream file(experiment_dir + "/results_" + string(config[IP_ADDR]) + ".dat");
    cout << "Writing to " << experiment_dir << "/results.dat" << endl;
    vector<uint32_t> tmp;
    vector<vector<uint32_t>> msLatencyLists(config[THREADS], tmp);
    int client_id = config[CLIENT_ID];
    int num_threads = config[THREADS];

    // initialize openssl library and register algorithms
    OpenSSL_add_all_algorithms();
    ERR_load_BIO_strings();
    ERR_load_crypto_strings();
    SSL_load_error_strings();
 
    chrono::steady_clock::time_point begin_exp = chrono::steady_clock::time_point::max();
    //Lock lock;
    //UniqueLock w_lock(lock);
    //w_lock.unlock();

    if (SSL_library_init() < 0)
    {
        debug_log::error(TLS_CLIENT
               "TLS client: could not initialize the OpenSSL library !\n");
        goto done;
    }

    ret = 0;

    for (int i = 0; i < num_threads; i++) {
        //msLatencyLists.push_back(new vector<uint32_t>);
        int index = rand() % num_balancers;
        thread *t = new thread(requestThread, config[LB_ADDRS][index], client_id * num_threads + i, config[EXP_SEC], &msLatencyLists, config[BATCH_SZ], config[NUM_BLOCKS], &begin_exp);
        threads.push_back(t);
        printf("started thread %d for balancer %d\n", i, index);
        this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    /* Start experiment. */
    //w_lock.lock();
    begin_exp = chrono::steady_clock::now();
    //w_lock.unlock();

    for (int i = 0; i < config[THREADS]; i++) {
        threads[i]->join();
    }

    if (file.is_open()) {
        for (int i = 0; i < msLatencyLists.size(); i++) {
            for (int j = 0; j < msLatencyLists[i].size(); j++) {
                file << "1 " << to_string(msLatencyLists[i][j]) << endl;
            }
        }
    }
    file.close();

    printf("Finished client\n"); 

   /* client = new OramClient(grpc::CreateChannel(
                target_str, grpc::InsecureChannelCredentials()), 0);
    memset(block_bytes, 0xff, BLOCK_LEN);
    client->MakeReq(9, block_bytes, block_bytes, false);
    printf("----- starting second request ----- \n");
    memset(block_bytes, 0, BLOCK_LEN);
    client->MakeReq(9, block_bytes, block_bytes, true);*/
done:
    debug_log::info(TLS_CLIENT " %s\n", (ret == 0) ? "success" : "failed");
    return (ret);
}
