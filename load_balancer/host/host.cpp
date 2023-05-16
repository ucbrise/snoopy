// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include <openenclave/host.h>
#include <stdio.h>
#include "load_balancer_u.h"

#include <sched.h>
#include <pthread.h>

#include "log.h"

#include <grpcpp/grpcpp.h>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <fstream>

#include <boost/math/special_functions/lambert_w.hpp>

#include "../../build/common/oram.grpc.pb.h"
#include "../../common/common.h"
#include "../../common/crypto.h"
#include "../../common/json.hpp"
#include "host.h"

#define LOOP_OPTION "-server-in-loop"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using oram::OramRequest;
using oram::OramRequests;
using oram::OramResponse;
using oram::OramResponses;
using oram::Oram;

using json = nlohmann::json;

using boost::math::lambert_w0;

using namespace std;
using namespace lb_types;

LBServerImpl *server;
LBBatchDispatcher *batchDispatcher;
vector<ClientReq *> reqList;
mutex reqListLock;
oe_enclave_t *enclave;
int num_suborams;

oe_enclave_t* create_enclave(const char* enclave_path)
{
    oe_enclave_t* enclave = NULL;

    debug_log::info("Host: Enclave library %s\n", enclave_path);
    oe_result_t result = oe_create_load_balancer_enclave(
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
        debug_log::info(
            "Host: oe_create_remoteattestation_enclave failed. %s",
            oe_result_str(result));
    }
    else
    {
        debug_log::info("Host: Enclave successfully created.\n");
    }
    return enclave;
}

void terminate_enclave(oe_enclave_t* enclave)
{
    oe_terminate_enclave(enclave);
    debug_log::info("Host: Enclave successfully terminated.\n");
}

LBServerImpl::LBServerImpl() : client_id_ctr_(0), caller_map_(), sec_param_(128) {}

LBServerImpl::~LBServerImpl() {
    server_->Shutdown();
    cq_->Shutdown();
}

void LBServerImpl::Run(std::string server_addr) {
    ServerBuilder builder;
    builder.AddListeningPort(server_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    cq_ = builder.AddCompletionQueue();
    server_ = builder.BuildAndStart();
    std::cout << "Server listening on " << server_addr << std::endl;
    fflush(stdout);

    HandleRpcs();
}

int LBServerImpl::GetPaddedReqsPerSuboram(int incoming_reqs) {
    double mu = (double) incoming_reqs / (double) num_suborams;
    double alpha = log(((double) num_suborams) * pow(2.0, sec_param_));
    double rhs = alpha / (M_E * mu) - (1.0 / M_E);
    double epsilon = pow(M_E, lambert_w0(rhs) + 1.0) - 1.0;
    double result = mu * (1.0 + epsilon);
    return (int) ceil(result);
    // TODO: actually implement
    /*if (incoming_reqs == 1024)
        return 155;
    else if (incoming_reqs == 2048) {
        return 232;
    } else if (incoming_reqs == 4096) {
        return 366;
    } else if (incoming_reqs == 8192) {
        return 602;
    } else if (incoming_reqs == 16384) {
        return 1032;
    }
    return incoming_reqs;*/
}

void testGetPaddedReqsPerSuboram() {
    LBServerImpl lb;
    int save_num_suborams = num_suborams;
    num_suborams = 25;
    double res = lb.GetPaddedReqsPerSuboram(1024);
    printf("1024: got %f, needed 155\n", res);
    res = lb.GetPaddedReqsPerSuboram(2048);
    printf("2048: got %f, needed 232\n", res);
    res = lb.GetPaddedReqsPerSuboram(4096);
    printf("4096: got %f, needed 366\n", res);
    res = lb.GetPaddedReqsPerSuboram(8192);
    printf("8192: got %f, needed 602\n", res);
    res = lb.GetPaddedReqsPerSuboram(16384);
    printf("16384: got %f, needed 1032\n", res);
    num_suborams = save_num_suborams;
}

void LBServerImpl::ProcessBatch() {
    reqListLock.lock();
    if (reqList.size() == 0) {
        reqListLock.unlock();
        return;
    }
    debug_log::info("Processing batch\n");

    // TODO: actually have enclave process batch
    // Go through and process batch
    // TODO: deal with race condition with reqList
    int num_reqs = reqList.size();
    int pair_len = get_key_val_buf_sz();
    int reqs_per_suboram = GetPaddedReqsPerSuboram(num_reqs);
    if (reqs_per_suboram > num_reqs) {
        reqs_per_suboram = num_reqs;
    }
    int suboram_buf_len = get_key_val_buf_sz(reqs_per_suboram);
    int total_out_reqs = reqs_per_suboram * num_suborams;
    uint8_t **in_ct_arr = (uint8_t **)malloc(num_reqs * sizeof(uint8_t *));
    uint8_t **in_iv_arr = (uint8_t **)malloc(num_reqs * sizeof(uint8_t *));
    uint8_t **in_tag_arr = (uint8_t **)malloc(num_reqs * sizeof(uint8_t *));
    uint8_t **out_ct_arr = (uint8_t **)malloc(num_suborams * sizeof(uint8_t *));
    uint8_t **out_iv_arr = (uint8_t **)malloc(num_suborams * sizeof(uint8_t *));
    uint8_t **out_tag_arr = (uint8_t **)malloc(num_suborams * sizeof(uint8_t *));
    uint8_t **resp_ct_arr = (uint8_t **)malloc(num_suborams * sizeof(uint8_t *));
    uint8_t **resp_iv_arr = (uint8_t **)malloc(num_suborams * sizeof(uint8_t *));
    uint8_t **resp_tag_arr = (uint8_t **)malloc(num_suborams * sizeof(uint8_t *));
    uint8_t **client_ct_arr = (uint8_t **)malloc(num_reqs * sizeof(uint8_t *));
    uint8_t **client_iv_arr = (uint8_t **)malloc(num_reqs * sizeof(uint8_t *));
    uint8_t **client_tag_arr = (uint8_t **)malloc(num_reqs * sizeof(uint8_t *));
    uint32_t *in_client_id_arr = (uint32_t *)malloc(num_reqs * sizeof(uint32_t));
    set<uint32_t> client_id_set;

    debug_log::info("Batch: %d real_requests, %d total_requests, %d reqs_per_suboram\n", num_reqs, reqs_per_suboram*num_suborams, reqs_per_suboram);

    for (int i = 0; i < num_reqs; i++) {
        /*in_ct_arr[i] = (uint8_t *)malloc(pair_len);
        memcpy(in_ct_arr[i], reqList[i]->ct_, pair_len);
        in_iv_arr[i] = (uint8_t *)malloc(IV_LEN);
        memcpy(in_iv_arr[i], reqList[i]->iv_, IV_LEN);
        in_tag_arr[i] = (uint8_t *)malloc(TAG_LEN);
        memcpy(in_tag_arr[i], reqList[i]->tag_, TAG_LEN);*/
        in_ct_arr[i] = reqList[i]->ct_;
        in_iv_arr[i] = reqList[i]->iv_;
        in_tag_arr[i] = reqList[i]->tag_;
        in_client_id_arr[i] = reqList[i]->caller_->GetClientID();
        client_ct_arr[i] = (uint8_t *)malloc(pair_len);
        client_iv_arr[i] = (uint8_t *)malloc(IV_LEN);
        client_tag_arr[i] = (uint8_t *)malloc(TAG_LEN);
        client_id_set.insert(reqList[i]->caller_->GetClientID());
    }

    for (int i = 0; i < num_suborams; i++) {
        out_ct_arr[i] = (uint8_t *)malloc(suboram_buf_len);
        out_iv_arr[i] = (uint8_t *)malloc(IV_LEN);
        out_tag_arr[i] = (uint8_t *)malloc(TAG_LEN);
        resp_ct_arr[i] = (uint8_t *)malloc(suboram_buf_len);
        resp_iv_arr[i] = (uint8_t *)malloc(IV_LEN);
        resp_tag_arr[i] = (uint8_t *)malloc(TAG_LEN);
    }
    reqListLock.unlock();

    //debug_log::info("Going to create batch in enclave\n");
    chrono::steady_clock::time_point t1 = chrono::steady_clock::now();

    create_batch(enclave,in_ct_arr, in_iv_arr, in_tag_arr, in_client_id_arr, out_ct_arr,
        out_iv_arr, out_tag_arr, num_reqs, reqs_per_suboram
    );

    chrono::steady_clock::time_point t2 = chrono::steady_clock::now();

    //debug_log::info("Sending requests to suborams\n");
    batchDispatcher->SendAllBatches(out_ct_arr, out_iv_arr, out_tag_arr, resp_ct_arr, resp_iv_arr, resp_tag_arr, reqs_per_suboram);

    chrono::steady_clock::time_point t3 = chrono::steady_clock::now();
    //debug_log::info("Got responses from suborams, going to match them\n");
    match_responses_to_reqs(enclave, resp_ct_arr, resp_iv_arr, resp_tag_arr, in_client_id_arr, client_ct_arr, client_iv_arr, client_tag_arr);

    chrono::steady_clock::time_point t4 = chrono::steady_clock::now();
    cout << "total: " << std::chrono::duration_cast<std::chrono::microseconds>(t4-t1).count() << endl;
    cout << "\t create_batch: " << std::chrono::duration_cast<std::chrono::microseconds>(t2-t1).count() << endl;
    cout << "\t send_all_batches: " << std::chrono::duration_cast<std::chrono::microseconds>(t3-t2).count() << endl;
    cout << "\t match_responses: " << std::chrono::duration_cast<std::chrono::microseconds>(t4-t3).count() << endl;
    //debug_log::info("Matched responses from suborams\n");
    reqListLock.lock();
    caller_map_lock_.lock();
    for (int i = 0; i < num_reqs; i++) {
        caller_map_[in_client_id_arr[i]]->EnqueueResponse(client_ct_arr[i], client_iv_arr[i], client_tag_arr[i]);
        //caller_map_.erase(in_client_id_arr[i]);
    }
    set<uint32_t>::iterator it = client_id_set.begin();
    while (it != client_id_set.end()) {
        caller_map_[*it]->Respond();
        caller_map_.erase(*it);
        it++;
    }
    caller_map_lock_.unlock();
    reqList.erase(reqList.begin(), reqList.begin() + num_reqs);
    reqListLock.unlock();

    for (int i = 0; i < num_reqs; i++) {
        free(client_ct_arr[i]);
        free(client_iv_arr[i]);
        free(client_tag_arr[i]);
    }

    for (int i = 0; i < num_suborams; i++) {
        free(out_ct_arr[i]);
        free(out_iv_arr[i]);
        free(out_tag_arr[i]);
        free(resp_ct_arr[i]);
        free(resp_iv_arr[i]);
        free(resp_tag_arr[i]);
    }

    free(in_ct_arr);
    free(in_iv_arr);
    free(in_tag_arr);
    free(out_ct_arr);
    free(out_iv_arr);
    free(out_tag_arr);
    free(resp_ct_arr);
    free(resp_iv_arr);
    free(resp_tag_arr);
    free(client_ct_arr);
    free(client_iv_arr);
    free(client_tag_arr);
    free(in_client_id_arr);
}

uint32_t LBServerImpl::GetNextClientID() {
    client_id_ctr_lock_.lock();
    uint32_t next = client_id_ctr_++;
    client_id_ctr_lock_.unlock();
    return next;
}

void LBServerImpl::AddToCallerMap(uint32_t client_id, LBServerImpl::CallData *call_data) {
    caller_map_lock_.lock();
    caller_map_[client_id] = call_data;
    caller_map_lock_.unlock();
}

ClientReq::ClientReq(uint8_t *ct, uint8_t *iv, uint8_t *tag, LBServerImpl::CallData *caller)
    : caller_(caller) {
    ct_ = ct;
    iv_ = iv;
    tag_ = tag;    
}

ClientReq::~ClientReq() {
    free(ct_);
    free(iv_);
    free(tag_);
}
                
LBServerImpl::CallData::CallData(Oram::AsyncService *service, ServerCompletionQueue *cq)
            : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE) {
    Proceed();
}

uint32_t LBServerImpl::CallData::GetClientID() {
    return client_id_;
}

void LBServerImpl::CallData::Proceed() {
    if (status_ == CREATE) {
        status_ = PROCESS;
        service_->RequestAccessKey(&ctx_, &request_, &responder_, cq_, cq_,
                this);
    } else if (status_ == PROCESS) {
        //uint32_t new_client_id = server->GetNextClientID();
        server->AddToCallerMap(request_.client_id(), this);
        client_id_ = request_.client_id();
        new CallData(service_, cq_);
        // TODO: actual processing
        reqListLock.lock();
        for (int i = 0; i < request_.ct_size(); i++) {
            uint8_t *ct = (uint8_t *)malloc(get_key_val_buf_sz());
            uint8_t *iv = (uint8_t *)malloc(IV_LEN);
            uint8_t *tag = (uint8_t *)malloc(TAG_LEN);;
            memcpy(ct, request_.ct(i).c_str(), get_key_val_buf_sz());
            memcpy(iv, request_.iv(i).c_str(), IV_LEN);
            memcpy(tag, request_.tag(i).c_str(), TAG_LEN);
            ClientReq *creq = new ClientReq(ct, iv, tag, this);
            // TODO: need to create multiple client reqs
            reqList.push_back(creq);
        }
        reqListLock.unlock();
        lock_.lock();
        //reply_.set_key(request_.key());
        //reply_.set_data(request_.new_data());
        //status_ = FINISH;
        //responder_.Finish(reply_, Status::OK, this);
    } else {
        GPR_ASSERT(status_ == FINISH);
        delete this;
    }
}

void LBServerImpl::CallData::EnqueueResponse(uint8_t *ct, uint8_t *iv, uint8_t *tag) {
    reply_.add_ct(ct, get_key_val_buf_sz());
    reply_.add_iv(iv, IV_LEN);
    reply_.add_tag(tag, TAG_LEN);
}

void LBServerImpl::CallData::Respond() {
//void LBServerImpl::CallData::Respond(uint8_t *ct, uint8_t *iv, uint8_t *tag) {
    /*reply_.set_ct(ct, get_key_val_buf_sz());
    reply_.set_iv(iv, IV_LEN);
    reply_.set_tag(tag, TAG_LEN);*/
    // TODO: need to set repeated fields appropriately
    status_ = FINISH;
    responder_.Finish(reply_, Status::OK, this);
    lock_.unlock();
}

// can be run in multiple threads
void LBServerImpl::HandleRpcs() {
    new CallData(&service_, cq_.get());
    void *tag;
    bool ok;
    while (true) {
        GPR_ASSERT(cq_->Next(&tag, &ok));
        GPR_ASSERT(ok);
        static_cast<CallData*>(tag)->Proceed();
    }
}

LBDispatcher::LBDispatcher(shared_ptr<Channel> channel, uint32_t id)
    : stub_(Oram::NewStub(channel)), id_(id) {}

int LBDispatcher::SendBatch(uint8_t *in_ct, uint8_t *in_iv, uint8_t *in_tag, uint8_t *out_ct, uint8_t *out_iv, uint8_t *out_tag, int len, bool doCopy) {
    OramRequests reqs;
    OramResponses resps;
    ClientContext ctx;
    Status status;

    reqs.set_ct(in_ct, get_key_val_buf_sz(len));
    reqs.set_iv(in_iv, IV_LEN);
    reqs.set_tag(in_tag, TAG_LEN);
    reqs.set_len(len);
    reqs.set_balancer_id(id_);

    status = stub_->BatchedAccessKeys(&ctx, reqs, &resps);
    if (status.ok()) {
        if (doCopy) {
            memcpy(out_ct, resps.ct().c_str(), get_key_val_buf_sz(len));
            memcpy(out_iv, resps.iv().c_str(), IV_LEN);
            memcpy(out_tag, resps.tag().c_str(), TAG_LEN);
        }
        return OKAY;
    } else {
        debug_log::error("ERROR receiving responses from suboram\n");
    }
    return ERROR;
}

LBBatchDispatcher::LBBatchDispatcher(vector<vector<string>> suboramIPs, uint32_t id) {
    for (int i = 0; i < suboramIPs.size(); i++) {
        dispatchers.push_back(vector<LBDispatcher*>(suboramIPs[i].size()));
        for (int j = 0; j < suboramIPs[i].size(); j++) {
            dispatchers[i][j] = new LBDispatcher(grpc::CreateChannel(
                           suboramIPs[i][j], grpc::InsecureChannelCredentials()), id);
        }
    }
}

int LBBatchDispatcher::SendAllBatches(uint8_t **in_ct_arr, uint8_t **in_iv_arr, uint8_t **in_tag_arr, uint8_t **out_ct_arr, uint8_t **out_iv_arr, uint8_t **out_tag_arr, int batch_sz) {
    vector<thread*> threads;
    for (int i = 0; i < dispatchers.size(); i++) {
        for (int j = 0; j < dispatchers[i].size(); j++) {
            thread *t = new thread(&LBDispatcher::SendBatch, dispatchers[i][j],
                in_ct_arr[i], in_iv_arr[i], in_tag_arr[i],
                out_ct_arr[i], out_iv_arr[i], out_tag_arr[i],
                batch_sz, j == 0);
            threads.push_back(t);
        }

    }
    int len = threads.size();
    for (int i = 0; i < len; i++) {
        threads[i]->join();
    }
}
        
void batch_timer(int epoch_time) {
    struct sched_param param;
    param.sched_priority = 99;  // max
    //pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    while(true) {
        chrono::steady_clock::time_point tStart = chrono::steady_clock::now();
        server->ProcessBatch();
        chrono::steady_clock::time_point tEnd = chrono::steady_clock::now();
        int msElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();
        if (epoch_time - msElapsed > 0) {
            this_thread::sleep_for(chrono::milliseconds(epoch_time - msElapsed));
        }
        server->ProcessBatch();
    }    
}

int main(int argc, const char* argv[])
{
    debug_log::set_name("lb");
    oe_result_t result = OE_OK;
    int ret = 1;
    char* server_port = NULL;
    char* suboram_port = NULL;
    char* suboram_name = NULL; // TODO: SET THIS
    bool keep_server_up = false;
    //num_suborams = 1;
    //num_suborams = 25;
    //vector<string> suboramIPs {"127.0.0.1:12346"};
    ifstream config_stream(argv[2]);
    json config;
    config_stream >> config;
    //string config_filename(argv[2]);
    //Json::Value config = read_config(config_filename);
    vector<vector<string>> suboramIPs;
    int balancer_id = config[BALANCER_ID];
    for (int i = 0; i < config[SUBORAM_ADDRS].size(); i++) {
        suboramIPs.push_back(vector<string>(config[SUBORAM_ADDRS][i].size()));
        for (int j = 0; j < config[SUBORAM_ADDRS][i].size(); j++) {
            suboramIPs[i][j] = config[SUBORAM_ADDRS][i][j];
        }
    }
    num_suborams = suboramIPs.size();

    server = new LBServerImpl();

    thread t(batch_timer, config[EPOCH_MS]);

    debug_log::info("Host: Creating an tls client enclave\n");
    enclave = create_enclave(argv[1]);
    std::vector<std::thread> enclave_threads;
    if (enclave == NULL)
    {
        goto exit;
    }

    batchDispatcher = new LBBatchDispatcher(suboramIPs, balancer_id);
    init_load_balancer(enclave, suboramIPs.size(), config[NUM_BLOCKS], config[THREADS]);
    for (int i = 1; i < config[THREADS]; i++) {
        enclave_threads.push_back(spawn_enclave_thread_on_cpu(i, start_worker_loop, enclave, i));
    }
    debug_log::info("starting server\n");
    if (config[MODE] == SERVER) {
        server->Run("0.0.0.0:" + string(config[LISTENING_PORT]));
    } else if (config[MODE] == BENCH_MAKE_BATCH) {
        server->BenchCreateBatch(true);
    } else if (config[MODE] == BENCH_MATCH_RESPS) {
        server->BenchCreateBatch(false);
    }
    
    //t.join();

exit:

    debug_log::info("Host: Terminating enclaves\n");
    if (enclave)
        stop_workers(enclave);
        for (auto &et : enclave_threads) {
            et.join();
        }
        terminate_enclave(enclave);

    debug_log::info("Host:  %s \n", (ret == 0) ? "succeeded" : "failed");
    return ret;
}
// If want to be able to run this, need to pass ciphertexts to enclave not raw key-val
// pairs
void LBServerImpl::BenchCreateBatch(bool makeBatch) {
    uint32_t replay_counter = 1;

    unsigned char *comm_key = (unsigned char *)"01234567891234567891234567891234";
    ofstream make_batch_file("micro_balancer_make_batch_" + std::to_string(num_suborams) + ".dat");
    ofstream match_resps_file("micro_balancer_match_resps_" + std::to_string(num_suborams) + ".dat");

    for (int i = 6; i < 15; i++) {
        int num_reqs = 1 << i;
    
        int pair_len = get_key_val_buf_sz();
        int reqs_per_suboram = GetPaddedReqsPerSuboram(num_reqs);
        int suboram_buf_len = get_key_val_buf_sz(reqs_per_suboram);
        int total_out_reqs = reqs_per_suboram * num_suborams;

        uint8_t **in_ct_arr = (uint8_t **)malloc(num_reqs * sizeof(uint8_t *));
        uint8_t **in_iv_arr = (uint8_t **)malloc(num_reqs * sizeof(uint8_t *));
        uint8_t **in_tag_arr = (uint8_t **)malloc(num_reqs * sizeof(uint8_t *));
        uint8_t **out_ct_arr = (uint8_t **)malloc(num_suborams * sizeof(uint8_t *));
        uint8_t **out_iv_arr = (uint8_t **)malloc(num_suborams * sizeof(uint8_t *));
        uint8_t **out_tag_arr = (uint8_t **)malloc(num_suborams * sizeof(uint8_t *));
        uint8_t **resp_ct_arr = (uint8_t **)malloc(num_suborams * sizeof(uint8_t *));
        uint8_t **resp_iv_arr = (uint8_t **)malloc(num_suborams * sizeof(uint8_t *));
        uint8_t **resp_tag_arr = (uint8_t **)malloc(num_suborams * sizeof(uint8_t *));
        uint8_t **client_ct_arr = (uint8_t **)malloc(num_reqs * sizeof(uint8_t *));
        uint8_t **client_iv_arr = (uint8_t **)malloc(num_reqs * sizeof(uint8_t *));
        uint8_t **client_tag_arr = (uint8_t **)malloc(num_reqs * sizeof(uint8_t *));
        uint32_t *in_client_id_arr = (uint32_t *)malloc(num_reqs * sizeof(uint32_t));
        set<uint32_t> client_id_set;

        for (int j = 0; j < num_reqs; j++) {
            in_ct_arr[j] = (uint8_t *)malloc(pair_len);
            in_iv_arr[j] = (uint8_t *)malloc(IV_LEN);
            in_tag_arr[j] = (uint8_t *)malloc(TAG_LEN);
            encrypt_read_key(comm_key, in_ct_arr[j], in_iv_arr[j], in_tag_arr[j], 1, &replay_counter, false);
            in_client_id_arr[j] = 1;
            client_ct_arr[j] = (uint8_t *)malloc(pair_len);
            client_iv_arr[j] = (uint8_t *)malloc(IV_LEN);
            client_tag_arr[j] = (uint8_t *)malloc(TAG_LEN);
            client_id_set.insert(j);
        }

        for (int j = 0; j < num_suborams; j++) {
            out_ct_arr[j] = (uint8_t *)malloc(suboram_buf_len);
            out_iv_arr[j] = (uint8_t *)malloc(IV_LEN);
            out_tag_arr[j] = (uint8_t *)malloc(TAG_LEN);
            resp_ct_arr[j] = (uint8_t *)malloc(suboram_buf_len);
            resp_iv_arr[j] = (uint8_t *)malloc(IV_LEN);
            resp_tag_arr[j] = (uint8_t *)malloc(TAG_LEN);
            uint8_t **data_arr = (uint8_t **)malloc(reqs_per_suboram * sizeof(uint8_t *));
            uint32_t *key_arr = (uint32_t *)malloc(reqs_per_suboram * sizeof(uint32_t));
            for (int k = 0; k < reqs_per_suboram; k++) {
                data_arr[k] = (uint8_t *)malloc(BLOCK_LEN);
                key_arr[k] = 1;
            }
            encrypt_key_val_pairs(comm_key, resp_ct_arr[j], resp_iv_arr[j], resp_tag_arr[j], key_arr, data_arr, reqs_per_suboram, &replay_counter, false);
            for (int k = 0; k < reqs_per_suboram; k++) {
                free(data_arr[k]);
            }
            free(data_arr);
            free(key_arr);
        }

        chrono::steady_clock::time_point t1 = chrono::steady_clock::now();

        create_batch(enclave,
            in_ct_arr, in_iv_arr, in_tag_arr, in_client_id_arr, out_ct_arr,
            out_iv_arr, out_tag_arr, num_reqs, reqs_per_suboram
        );

        chrono::steady_clock::time_point t2 = chrono::steady_clock::now();

        match_responses_to_reqs(enclave, resp_ct_arr, resp_iv_arr, resp_tag_arr, in_client_id_arr, client_ct_arr, client_iv_arr, client_tag_arr);
    
        chrono::steady_clock::time_point t3 = chrono::steady_clock::now();

        replay_counter++;

        for (int j = 0; j < num_reqs; j++) {
            free(in_ct_arr[j]);
            free(in_iv_arr[j]);
            free(in_tag_arr[j]);
            free(client_ct_arr[j]);
            free(client_iv_arr[j]);
            free(client_tag_arr[j]);
        }
        for (int j = 0; j < num_suborams; j++) {
            free(out_ct_arr[j]);
            free(out_iv_arr[j]);
            free(out_tag_arr[j]);
            free(resp_ct_arr[j]);
            free(resp_iv_arr[j]);
            free(resp_tag_arr[j]);
        }
        free(in_ct_arr);
        free(in_iv_arr);
        free(in_tag_arr);
        free(client_ct_arr);
        free(client_iv_arr);
        free(client_tag_arr);
        free(in_client_id_arr);
        free(out_ct_arr);
        free(out_iv_arr);
        free(out_tag_arr);
        free(resp_ct_arr);
        free(resp_iv_arr);
        free(resp_tag_arr);

        if (makeBatch) {
            cout << num_suborams << " " << num_reqs << " " << std::chrono::duration_cast<std::chrono::microseconds>(t2-t1).count() << endl;
        } else {
            cout << num_suborams << " " << num_reqs << " " << std::chrono::duration_cast<std::chrono::microseconds>(t3-t2).count() << endl;
        }
        /*
        printf("Time to create batch for %d blocks: %f us\n", num_reqs, ((double)std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count()));
        make_batch_file << num_suborams << " " << num_reqs << " " << std::chrono::duration_cast<std::chrono::microseconds>(t2-t1).count() << endl;
        printf("Time to match up requests for %d blocks: %f us\n", num_reqs, ((double)std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count()));
        match_resps_file << num_suborams << " " << num_reqs << " " << std::chrono::duration_cast<std::chrono::microseconds>(t3-t2).count() << endl;*/
    }
    make_batch_file.close();
    match_resps_file.close();
}
