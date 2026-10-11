#include "uhrwerk/parallel.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <mutex>
#include <thread>

namespace uhrwerk {

unsigned ThreadPool::resolve_threads(unsigned requested) {
    if (requested != 0) {
        return requested;
    }
    const unsigned hardware = std::thread::hardware_concurrency();
    return hardware == 0 ? 1u : hardware;
}

ThreadPool::ThreadPool(unsigned threads) {
    const unsigned total = resolve_threads(threads);
    workers_.reserve(total - 1);
    for (unsigned i = 1; i < total; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();
    for (std::thread& worker : workers_) {
        worker.join();
    }
}

void ThreadPool::run(std::size_t count, std::size_t chunk, const ChunkFn& fn) {
    if (count == 0) {
        return;
    }
    if (chunk == 0) {
        chunk = 1;
    }
    const std::size_t chunks = (count + chunk - 1) / chunk;
    if (workers_.empty() || chunks < 2) {
        for (std::size_t begin = 0; begin < count; begin += chunk) {
            fn(begin, std::min(count, begin + chunk));
        }
        return;
    }

    {
        const std::lock_guard<std::mutex> lock(mutex_);
        fn_ = &fn;
        count_ = count;
        chunk_ = chunk;
        next_chunk_.store(0, std::memory_order_relaxed);
        error_ = nullptr;
        active_ = static_cast<unsigned>(workers_.size());
        ++generation_;
    }
    wake_.notify_all();

    work();

    std::exception_ptr error;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        done_.wait(lock, [this] { return active_ == 0; });
        fn_ = nullptr;
        error = error_;
        error_ = nullptr;
    }
    if (error) {
        std::rethrow_exception(error);
    }
}

void ThreadPool::worker_loop() {
    std::uint64_t seen = 0;
    for (;;) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [&] { return stop_ || generation_ != seen; });
            if (stop_) {
                return;
            }
            seen = generation_;
        }
        work();
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            if (--active_ == 0) {
                done_.notify_one();
            }
        }
    }
}

void ThreadPool::work() {
    for (;;) {
        const std::size_t index = next_chunk_.fetch_add(1, std::memory_order_relaxed);
        const std::size_t begin = index * chunk_;
        if (begin >= count_) {
            return;
        }
        try {
            (*fn_)(begin, std::min(count_, begin + chunk_));
        } catch (...) {
            const std::lock_guard<std::mutex> lock(mutex_);
            if (!error_) {
                error_ = std::current_exception();
            }
        }
    }
}

} // namespace uhrwerk
