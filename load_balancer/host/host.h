#ifndef _LB_HOST_
#define _LB_HOST_

#include <mutex>
#include <../../common/bucket_sort.h>
#include <../../common/block.h>

using grpc::Channel;
using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using oram::OramRequest;
using oram::OramResponse;
using oram::Oram;

using namespace std;
using namespace lb_types;

class LBServerImpl {
    public:
        LBServerImpl();
        ~LBServerImpl();
        void Run(std::string server_addr);
        void ProcessBatch();
        class CallData {
            public:
                CallData(Oram::AsyncService *service, ServerCompletionQueue *cq);
                void Proceed();
                void Respond();
                void EnqueueResponse(uint8_t *ct, uint8_t *iv, uint8_t *tag);
                uint32_t GetClientID();
            private:
                Oram::AsyncService *service_;
                ServerCompletionQueue *cq_;
                ServerContext ctx_;
                OramRequest request_;
                OramResponse reply_;
                ServerAsyncResponseWriter<OramResponse> responder_;
                enum CallStatus { CREATE, PROCESS, FINISH };
                CallStatus status_;
                mutex lock_;
                uint32_t client_id_;
        };
        void HandleRpcs();
        uint32_t GetNextClientID();
        void AddToCallerMap(uint32_t client_id, LBServerImpl::CallData *call_data);
        int GetPaddedReqsPerSuboram(int incoming_reqs);
        void BenchCreateBatch(bool makeBatch);
    private:
        std::unique_ptr<ServerCompletionQueue> cq_;
        Oram::AsyncService service_;
        std::unique_ptr<Server> server_;
        uint32_t client_id_ctr_;
        mutex client_id_ctr_lock_;
        map<uint32_t, LBServerImpl::CallData *>caller_map_;
        mutex caller_map_lock_;
        int sec_param_;
};

class ClientReq {
    public:
        ClientReq(uint8_t *ct, uint8_t *iv, uint8_t *tag, LBServerImpl::CallData *caller);
        ~ClientReq();
        uint8_t *ct_;
        uint8_t *iv_;
        uint8_t *tag_;
        LBServerImpl::CallData *caller_;
};

class LBDispatcher {
    public:
        LBDispatcher(shared_ptr<Channel> channel, uint32_t id);
        int SendBatch(uint8_t *in_ct, uint8_t *in_iv, uint8_t *in_tag, uint8_t *out_ct, uint8_t *out_iv, uint8_t *out_tag, int len, bool doCopy);
    private:
        unique_ptr<Oram::Stub> stub_;
        uint32_t id_;
};

class LBBatchDispatcher {
    public:
        LBBatchDispatcher(vector<vector<string>> suboramIPs, uint32_t id);
        int SendAllBatches(uint8_t **in_ct_arr, uint8_t **in_iv_arr, uint8_t **in_tag_arr, uint8_t **out_ct_arr, uint8_t **out_iv_arr, uint8_t **out_tag_arr, int batch_sz);
    private:
        vector<vector<LBDispatcher*>> dispatchers;
};

#endif
