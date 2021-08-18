#include "obl_primitives.h"
#include <mutex>
#include <condition_variable>

namespace pos {
  enum ThreadFn {
    cmp,
    stop
  };

  struct thread_state {
    uint32_t k;
    uint32_t j;
    ThreadFn fn;
    int curr_iter;
    int n_done;
  };

  extern std::mutex m;
  extern std::condition_variable cv;
  extern thread_state state;

  void notify_threads(ThreadFn fn);
  void wait_for_threads(int num_threads);
}

std::pair<int, int> get_cutoffs_for_thread(int thread_id, int total, int n_threads);

namespace detail {
template <typename T, typename Comparator>
inline void imperative_o_sort_cmp(T *arr, Comparator cmp, int start, int end) {
  uint32_t i;
    for (i = (uint32_t) start; i < (uint32_t) end; i++) {
        uint32_t ij = i ^ pos::state.j;
        if (ij > i) {
            if ((i & pos::state.k) == 0) {
                bool pred = cmp(arr[ij], arr[i]);
                // These array accesses are oblivious because the indices are
                // deterministic
                T tmp = arr[i];
                arr[i] = ObliviousChoose(pred, arr[ij], arr[i]);
                arr[ij] = ObliviousChoose(pred, tmp, arr[ij]);
            } else {
                bool pred = cmp(arr[i], arr[ij]);
                // These array accesses are oblivious because the indices are
                // deterministic
                T tmp = arr[i];
                arr[i] = ObliviousChoose(pred, arr[ij], arr[i]);
                arr[ij] = ObliviousChoose(pred, tmp, arr[ij]);
            }
        }
    }
}

template <typename T, typename Comparator>
inline void imperative_o_sort_thread(T *arr, size_t n, Comparator cmp, int n_threads, int thread_id) {
  auto bounds = get_cutoffs_for_thread(thread_id, n, n_threads);
  int next_iter = 1;
  while (true) {
    std::unique_lock<std::mutex> lk(pos::m);
    pos::cv.wait(lk, [next_iter, thread_id]{
        bool ready = pos::state.curr_iter == next_iter;
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
    pos::ThreadFn fn = pos::state.fn;
    if (fn == pos::ThreadFn::cmp) {
      imperative_o_sort_cmp(arr, cmp, bounds.first, bounds.second);
    }
    next_iter++;
    lk.lock();
    if (++pos::state.n_done == n_threads) {
      lk.unlock();
      pos::cv.notify_all();
    }
    if (fn == pos::ThreadFn::stop) {
      return;
    }
  }
}

// Imperative implementation of bitonic sorting network -- works only for powers
// of 2
template <typename T, typename Comparator>
inline void imperative_o_sort_parallel(T *arr, size_t n, Comparator cmp, int n_threads, int thread_id, bool adaptive) {
  if (adaptive && sizeof(T) * n <= 1343488) {   // Heuristic; cost of coordination is worse than just sorting on one thread
    n_threads = 1;
    if (thread_id > 0) {
      return;
    }
  }
  if (thread_id > 0) {
    return imperative_o_sort_thread(arr, n, cmp, n_threads, thread_id);
  }
  auto bounds = get_cutoffs_for_thread(thread_id, n, n_threads);
  pos::state.curr_iter = 0;
  uint32_t j, k;
  for (k = 2; k <= n; k = 2 * k) {
    for (j = k >> 1; j > 0; j = j >> 1) {
      pos::state.k = k;
      pos::state.j = j;
      pos::notify_threads(pos::ThreadFn::cmp);
      imperative_o_sort_cmp(arr, cmp, bounds.first, bounds.second);
      pos::wait_for_threads(n_threads);
    }
  }
  pos::notify_threads(pos::ThreadFn::stop);
  pos::wait_for_threads(n_threads);
}

template <typename T, typename Comparator>
inline void o_sort_parallel(T *arr, uint32_t low, uint32_t len, Comparator cmp, int n_threads, int thread_id, bool adaptive) {
  if (len > 1) {
    uint32_t m = greatest_power_of_two_less_than(len);
    if (m * 2 == len) {
      imperative_o_sort_parallel(arr + low, len, cmp, n_threads, thread_id, adaptive);
    } else {
      imperative_o_sort_parallel(arr + low, m, obl::reverse_cmp<T, Comparator>(cmp), n_threads, thread_id, adaptive);
      o_sort_parallel(arr, low + m, len - m, cmp, n_threads, thread_id, adaptive);
      if (thread_id == 0) {
        detail::imperative_o_merge(arr, low, len, cmp);
      }
    }
  }
}
}

template <typename Iter, typename Comparator>
inline void ObliviousSortParallel(Iter begin, Iter end, Comparator cmp, int num_threads, int thread_id) {
  using value_type = typename std::remove_reference<decltype(*begin)>::type;
  value_type *array = &(*begin);
  return detail::o_sort_parallel<value_type, Comparator>(array, 0, end - begin, cmp, num_threads, thread_id, true);
}

template <typename Iter, typename Comparator>
inline void ObliviousSortParallelNonAdaptive(Iter begin, Iter end, Comparator cmp, int num_threads, int thread_id) {
  using value_type = typename std::remove_reference<decltype(*begin)>::type;
  value_type *array = &(*begin);
  return detail::o_sort_parallel<value_type, Comparator>(array, 0, end - begin, cmp, num_threads, thread_id, false);
}
