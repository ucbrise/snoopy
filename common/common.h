// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.
#ifndef _COMMON_
#define _COMMON_

#include <iostream>
#include <stdbool.h>
#include <stdio.h>
#include <type_traits>
#include <thread>

#define ADD_TEST_CHECKING

#define BLOCK_LEN 160

/* For log */
#define TLS_CLIENT "TLS client: "
#define TLS_SERVER "TLS server: "
#define SUBORAM "SubORAM: "
#define LB_CLIENT "Load balancer client: "
#define LB_SERVER "Load balancer server: "

/* For JSON config files */
#define LISTENING_PORT "listening_port"
#define LB_ADDRS "lb_addrs"
#define SUBORAM_ADDRS "suboram_addrs"
#define NUM_BLOCKS "num_blocks"
#define EPOCH_MS "epoch_ms"
#define THREADS "threads"
#define RUN_NAME "run_name"
#define EXP_SEC "exp_sec"
#define EXP_DIR "experiment_dir"
#define NUM_BALANCERS "num_balancers"
#define BATCH_SZ "batch_size"
#define IP_ADDR "ip_addr"
#define BALANCER_ID "balancer_id"
#define OUR_PROTOCOL "our_protocol"
#define OBLIX_PROTOCOL "oblix_protocol"
#define PROTOCOL_TYPE "protocol_type"
#define MODE "mode"
#define BENCH_SORT "bench_sort"
#define BENCH_PROCESS_BATCH "bench_process_batch"
#define SERVER "server"
#define SORT_TYPE "sort_type"
#define BUCKET_SORT "bucket_sort"
#define BITONIC_SORT "bitonic_sort"
#define BITONIC_SORT_NONADAPTIVE "bitonic_sort_nonadaptive"
#define BUFFERED_BUCKET_SORT "buffered_bucket_sort"
#define HASH_TABLE "hash_table"
#define NUM_SUBORAMS "num_suborams"
#define SUBORAM_ID "suboram_id"
#define CLIENT_ID "client_id"
#define BENCH_MAKE_BATCH "bench_make_batch"
#define BENCH_MATCH_RESPS "bench_match_resps"
#define OBLIX_BASELINE "oblix_baseline"
#define OBLIX_BASELINE_160 "160"
#define OBLIX_BASELINE_10 "10"
#define OBLIX_BASELINE_100 "100"
#define OBLIX_BASELINE_1000 "1000"
#define OBLIX_BASELINE_10000 "10000"
#define OBLIX_BASELINE_100000 "100000"
#define OBLIX_BASELINE_1000000 "1000000"

#define CLIENT_PAYLOAD "GET / HTTP/1.0\r\n\r\n"
#define SERVER_PAYLOAD                                   \
    "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n" \
    "<h2>mbed TLS Test Server</h2>\r\n"                  \
    "<p>Successful connection : </p>\r\n"                \
    "A message from TLS server inside enclave\r\n"

#define CLIENT_PAYLOAD_SIZE strlen(CLIENT_PAYLOAD)
#define SERVER_PAYLOAD_SIZE strlen(SERVER_PAYLOAD)

/*
 * Return codes. For consistency with OpenSSL, we use
 * non-zero values to denote success.
 */
#define OKAY 1
#define ERROR 0

/* Check a call that should return OKAY. */
#define CHECK_C(expr) do {\
  (rv = (expr));\
  if (rv != OKAY) {\
    goto cleanup;\
  }\
} while(false);

/* Check an allocation that should return non-NULL.*/
#define CHECK_A(expr) do {\
  (rv = ((expr) != NULL));\
  if (rv != OKAY) {\
    goto cleanup;\
  }\
} while(false);


// Assert that T is a specialization of the template TT, i.e., T = TT<foo>
template<typename T, template<typename> class TT>
struct is_instantiation_of : std::false_type { };

template<typename T, template<typename> class TT>
struct is_instantiation_of<TT<T>, TT> : std::true_type { };

/* For testing */
#ifndef NDEBUG
#   define assertm(condition, message) \
    do { \
        if (! (condition)) { \
            std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
                      << " line " << __LINE__ << ": " << message << std::endl; \
            std::terminate(); \
        } \
    } while (false)
#else
#   define assertm(condition, message) do { } while (false)
#endif

# define assert_eq(actual, expected) assertm(expected == actual, "expected " << expected << " but got " << actual << "\n")
#endif

