// Tests für den Thread-Pool (seit M4).

#include "uhrwerk/parallel.hpp"

#include <atomic>
#include <cstddef>
#include <doctest/doctest.h>
#include <stdexcept>
#include <vector>

using uhrwerk::ThreadPool;

TEST_CASE("ThreadPool: jeder Index genau einmal, für viele Thread- und Paketgrößen") {
    for (const unsigned threads : {1u, 2u, 3u, 4u, 8u}) {
        ThreadPool pool(threads);
        CHECK(pool.threads() == threads);
        for (const std::size_t count : {std::size_t{0}, std::size_t{1}, std::size_t{7}, std::size_t{1000}}) {
            for (const std::size_t chunk :
                 {std::size_t{0}, std::size_t{1}, std::size_t{8}, std::size_t{5000}}) {
                std::vector<std::atomic<int>> hits(count);
                pool.run(count, chunk, [&](std::size_t begin, std::size_t end) {
                    CHECK(begin < end);
                    CHECK(end <= count);
                    for (std::size_t i = begin; i < end; ++i) {
                        hits[i].fetch_add(1);
                    }
                });
                for (std::size_t i = 0; i < count; ++i) {
                    INFO("threads=" << threads << " count=" << count << " chunk=" << chunk << " i=" << i);
                    CHECK(hits[i].load() == 1);
                }
            }
        }
    }
}

TEST_CASE("ThreadPool: viele Aufträge hintereinander") {
    ThreadPool pool(4);
    std::vector<long long> sums(64, 0);
    for (int round = 0; round < 2000; ++round) {
        pool.run(sums.size(), 4, [&](std::size_t begin, std::size_t end) {
            for (std::size_t i = begin; i < end; ++i) {
                sums[i] += static_cast<long long>(i); // jedes Element gehört genau einem Paket
            }
        });
    }
    for (std::size_t i = 0; i < sums.size(); ++i) {
        CHECK(sums[i] == static_cast<long long>(i) * 2000);
    }
}

TEST_CASE("ThreadPool: Ausnahme wird nach dem Auftrag weitergeworfen") {
    ThreadPool pool(3);
    std::atomic<int> done{0};
    CHECK_THROWS_AS(pool.run(100, 1,
                             [&](std::size_t begin, std::size_t) {
                                 if (begin == 42) {
                                     throw std::runtime_error("Paket 42");
                                 }
                                 done.fetch_add(1);
                             }),
                    std::runtime_error);
    CHECK(done.load() == 99); // alle anderen Pakete sind gelaufen
    // Der Pool bleibt benutzbar.
    std::atomic<int> after{0};
    pool.run(10, 1, [&](std::size_t, std::size_t) { after.fetch_add(1); });
    CHECK(after.load() == 10);
}

TEST_CASE("ThreadPool: 0 Threads heißt alle Kerne") {
    CHECK(ThreadPool::resolve_threads(0) >= 1);
    CHECK(ThreadPool::resolve_threads(5) == 5);
    ThreadPool pool(0);
    CHECK(pool.threads() == ThreadPool::resolve_threads(0));
}
