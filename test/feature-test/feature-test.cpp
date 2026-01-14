#include <future>
#include <iostream>
#include <memory>
#include <thread>


#include <BlockAllocator.hpp>
#include <LinearAllocator.hpp>
#include <RtlThreadPool.hpp>
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


// void allocator_test() {

//     std::vector<int, rtl::BlockAllocator<int>> vec;
//     auto newVec = vec;
//     vec.reserve(100);
//     vec.push_back(4);
//     newVec.push_back(12);
//     for (int i = 0; i < 20; i++) {
//         vec.push_back(i);
//         std::cout << i << " vec[i]" << vec[i] << '\n';
//         // std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     }
//     newVec.push_back(12);

//     // vec.push_back(4);
//     // vec.push_back(4);
//     // vec.push_back(4);
//     // vec.push_back(4);
// }


template <typename F, typename... Args>
auto invokeTest(F func, Args... args) -> std::invoke_result<F, Args...>::type {
    try {
        if (std::is_invocable_v<F, Args...> == false) {
            throw std::invalid_argument("Function is not invocable with the given arguments.");
        }
        auto result = std::invoke(std::forward<F>(func), std::forward<Args>(args)...);
        return std::move(result);
    } catch (const std::exception& exp) {
        std::cerr << exp.what() << '\n';
    }
}


template <typename F, typename... Args>
auto enqueue(F func, Args... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
    using ReturnType = typename std::invoke_result<F, Args...>::type; // this return type of the function
    auto tasking = std::make_shared<std::packaged_task<ReturnType()>>(std::bind(std::forward<F>(func), std::forward<Args>(args)...));
    auto returnFuture = tasking->get_future();
    auto closure = [tasking]() mutable {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        (*tasking)();
    };
    std::cout << "Starting thread...\n";
    std::thread thread(std::move(closure));
    thread.detach();
    return returnFuture;
}


int sum(int a, int b) {
    std::cout << "Calculating sum of " << a << " and " << b << "...\n";
    return a + b;
}


void vector_size_test() {
    std::vector<std::thread> threads;
    
}

int main() {
    // auto res = invokeTest(sum, 5, 7);
    std::cout << "Enqueuing task...\n";
    rtl::stp::ThreadPool thp(2, 4);
    auto futureResult = thp.put(sum, 5, 7);
    thp.put_periodic(500, sum, 5, 2);
    std::cout << "Doing other work in main thread...\n";
    int result = futureResult.get();
    std::cout << "The sum is: " << result << '\n';
    std::cout << "Main thread sleeping for 10 seconds to allow periodic tasks to run...\n";
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "Main thread slept\n";
    // allocator_test();
}
