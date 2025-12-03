#include <future>
#include <iostream>

#include <LinearAllocator.hpp>
#include <StaticThreadPool.h>
#include <UnboundedMPMCQueue.h>


void stp_test() {
    rtl::stp::StaticThreadPool pool(4);
    auto futureResult = pool.addTask<int>([]() { return 42; });
    auto futureResult2 = pool.addTask<std::string>([]() { return std::string("Hello, ThreadPool!"); });

    std::cout << "Waiting for the result..." << std::endl;
    int result = futureResult.get();
    std::cout << "The result is: " << result << std::endl;

    std::string result2 = futureResult2.get();
    std::cout << "The second result is: " << result2 << std::endl;

    pool.joinAll();
}


void allocator_test() {
    std::vector<int, rtl::LinearAllocator<int>> vec;
    vec.reserve(100);
    vec.push_back(4);
    for (int i = 0; i < 20; i++) {
        vec.push_back(i);
        std::cout << i << " vec[i]" << vec[i] << '\n';
        // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // vec.push_back(4);
    // vec.push_back(4);
    // vec.push_back(4);
    // vec.push_back(4);
}


int main() {
    allocator_test();
}
