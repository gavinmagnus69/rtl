#include <future>
#include <iostream>
#include <memory>
#include <source_location>
#include <thread>


#include <BlockAllocator.hpp>
#include <LinearAllocator.hpp>
#include <RtlThreadPool.hpp>
#include <StaticThreadPool.h>
#include <ThreadPoolExecutor.hpp>
#include <UnboundedMPMCQueue.h>


#include "spdlog/spdlog.h"

void thread_sleep(size_t ms = 10000) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}


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
    SPDLOG_INFO("Calculating sum of {} and {}\n", a, b);
    return a + b;
}

// working correctly
void test1() {
    SPDLOG_INFO("Enqueuing task...\n");
    rtl::stp::ThreadPool thp(2, 4);
    thp.put_periodic(500, std::move(sum), 5, 2);
    auto futureResult = thp.put(sum, 5, 7);
    SPDLOG_INFO("Doing other work in main thread...\n");
    int result = futureResult.get();
    SPDLOG_INFO("The sum is: {}\n", result);
    SPDLOG_INFO("Main thread sleeping for 10 seconds to allow periodic tasks to run...\n");
    // std::this_thread::sleep_for(std::chrono::seconds(10));
    thread_sleep();
    SPDLOG_INFO("Main thread slept\n");
}


void test2() {
    SPDLOG_INFO("{} {}\n", std::source_location::current().function_name(), std::source_location::current().file_name());
}

void test_threadpool_basic() {
    SPDLOG_INFO("ThreadPool basic test\n");
    rtl::stp::ThreadPool tp(2, 4);
    auto f1 = tp.put(sum, 1, 2);
    auto f2 = tp.put([]() { return std::string("ok"); });

    const int res1 = f1.get();
    const std::string res2 = f2.get();

    if (res1 != 3) {
        SPDLOG_ERROR("Expected 3, got {}\n", res1);
    }
    if (res2 != "ok") {
        SPDLOG_ERROR("Expected ok, got {}\n", res2);
    }
}

void test_threadpool_many_tasks() {
    SPDLOG_INFO("ThreadPool many tasks test\n");
    rtl::stp::ThreadPool tp(4, 8);
    constexpr int taskCount = 100;
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    futures.reserve(taskCount);

    for (int i = 0; i < taskCount; ++i) {
        futures.emplace_back(tp.put([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); }));
    }

    for (auto& f : futures) {
        f.get();
    }

    if (counter.load(std::memory_order_relaxed) != taskCount) {
        SPDLOG_ERROR("Expected {} tasks, got {}\n", taskCount, counter.load(std::memory_order_relaxed));
    }
}

void test_threadpool_exception() {
    SPDLOG_INFO("ThreadPool exception test\n");
    rtl::stp::ThreadPool tp(2, 4);
    auto f = tp.put([]() -> int {
        throw std::runtime_error("boom");
    });

    try {
        (void)f.get();
        SPDLOG_ERROR("Expected exception, got none\n");
    } catch (const std::exception& exp) {
        SPDLOG_INFO("Caught expected exception: {}\n", exp.what());
    }
}

// abort() called, unknown issue
void test3() {
    try {
        using namespace rtl::stp;
        auto tp = rtl::stp::makeThreadPoolExecutor();
        if (!tp) {
            SPDLOG_ERROR("nullptr");
            return;
        }
        TaskOptions opt{true, 500};
        auto ans = tp->submit(opt, sum, 5, 7);
        thread_sleep(5000);
        SPDLOG_INFO("thread finish\n");
        // ans.get();
        thread_sleep(500);
        SPDLOG_INFO("thread finish\n");
    } catch (const std::exception& exp) {
        SPDLOG_ERROR(exp.what());
    }
}


int main() {
    SPDLOG_INFO("feature-test");
    test1();
    test_threadpool_basic();
    test_threadpool_many_tasks();
    test_threadpool_exception();
    // test3(); // known abort, keep disabled
}
