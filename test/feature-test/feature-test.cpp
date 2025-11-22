#include <future>
#include <iostream>

#include <StaticThreadPool.h>
#include <UnboundedMPMCQueue.h>


int main() {
    rtl::stp::StaticThreadPool pool(4);
    auto futureResult = pool.addTask<int>([]() { return 42; });
    auto futureResult2 = pool.addTask<std::string>([]() { return std::string("Hello, ThreadPool!"); });

    std::cout << "Waiting for the result..." << std::endl;
    int result = futureResult.get();
    std::cout << "The result is: " << result << std::endl;

    std::string result2 = futureResult2.get();
    std::cout << "The second result is: " << result2 << std::endl;

    pool.joinAll();
    return 0;
}
