#include <atomic>
#include <barrier>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        std::cout << "usage: num_threads num_iterations\n";
        exit(EXIT_FAILURE);
    }

    const uint64_t num_threads = std::stoull(argv[1]);
    const uint64_t num_iterations = std::stoull(argv[2]);
    std::vector<std::thread> threads;
    std::barrier barrier(num_threads);
    std::atomic<uint64_t> count;

    auto work =
    [&]
    {
        barrier.arrive_and_wait();

        uint64_t res = 0;
        for (uint64_t i = 0; i < num_iterations; ++i) {
            res += 1;
        }
        count += res;
    };

    for (uint64_t i = 1; i < num_threads; ++i) {
        threads.emplace_back(work);
    }
    if (num_threads > 0) {
        work();
    }
    for (auto & t : threads) {
        t.join();
    }
    assert(count == num_iterations * num_threads);
}
