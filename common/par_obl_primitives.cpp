#include "par_obl_primitives.h"
#include <math.h>

namespace pos {
    std::mutex m;
    std::condition_variable cv;
    thread_state state;

    void notify_threads(ThreadFn fn) {
        {
            std::lock_guard<std::mutex> lk(m);
            state.fn = fn;
            state.curr_iter++;
            state.n_done = 1;
        }
        cv.notify_all();
    }

    void wait_for_threads(int num_threads) {
        {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [num_threads]{
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
}

std::pair<int, int> get_cutoffs_for_thread(int thread_id, int total, int n_threads) {
    int chunks = floor(total / n_threads);
    int start = chunks*thread_id;
    int end = start+chunks;
    if (thread_id + 1 == n_threads) {
        end = total;
    }
    // printf("[t %d] bounds: [%d, %d)\n", thread_id, start, end);
    return std::make_pair(start, end);
}