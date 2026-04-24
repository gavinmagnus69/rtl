#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <IExecutor.hpp>
#include <RtlThreadPool.hpp>
#include <ThreadPoolExecutor.hpp>

namespace {

using namespace std::chrono_literals;

class RejectingExecutor final : public rtl::stp::IExecutor {
protected:
  bool post(rtl::stp::Task &&, rtl::stp::TaskOptions) override {
    return false;
  }
};

void require(bool condition, const char *message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void test_threadpool_basic() {
  rtl::stp::ThreadPool tp(2, 4);
  auto f1 = tp.put([](int a, int b) { return a + b; }, 1, 2);
  auto f2 = tp.put([]() { return std::string("ok"); });

  require(f1.get() == 3, "basic one-shot task returned unexpected result");
  require(f2.get() == "ok", "basic string task returned unexpected result");
}

void test_threadpool_many_tasks() {
  rtl::stp::ThreadPool tp(4, 8);
  constexpr int task_count = 128;
  std::atomic<int> counter{0};
  std::vector<std::future<void>> futures;
  futures.reserve(task_count);

  for (int i = 0; i < task_count; ++i) {
    futures.emplace_back(tp.put(
        [&counter]() { counter.fetch_add(1, std::memory_order_relaxed); }));
  }

  for (auto &future : futures) {
    future.get();
  }

  require(counter.load(std::memory_order_relaxed) == task_count,
          "not all enqueued tasks completed");
}

void test_threadpool_exception_propagation() {
  rtl::stp::ThreadPool tp(2, 4);
  auto future = tp.put([]() -> int { throw std::runtime_error("boom"); });

  bool caught = false;
  try {
    (void)future.get();
  } catch (const std::runtime_error &exp) {
    caught = std::string(exp.what()) == "boom";
  }

  require(caught, "task exception was not propagated through future");
}

void test_threadpool_destructor_drains_accepted_tasks() {
  std::vector<std::future<int>> futures;
  futures.reserve(16);

  {
    rtl::stp::ThreadPool tp(2, 4);
    for (int i = 0; i < 16; ++i) {
      futures.emplace_back(tp.put([i]() {
        std::this_thread::sleep_for(5ms);
        return i;
      }));
    }
  }

  int sum = 0;
  for (auto &future : futures) {
    require(future.wait_for(0ms) == std::future_status::ready,
            "accepted task was not completed before destructor returned");
    sum += future.get();
  }

  require(sum == 120, "destructor-drain test observed wrong task results");
}

void test_threadpool_rejects_put_after_stop() {
  rtl::stp::ThreadPool tp(2, 4);
  tp.request_stop();

  bool threw = false;
  try {
    (void)tp.put([]() { return 7; });
  } catch (const std::runtime_error &) {
    threw = true;
  }

  require(threw, "put should reject once shutdown starts");
}

void test_threadpool_rejects_periodic_after_stop() {
  rtl::stp::ThreadPool tp(2, 4, 2);
  tp.request_stop();

  bool threw = false;
  try {
    tp.put_periodic(10, []() {});
  } catch (const std::runtime_error &) {
    threw = true;
  }

  require(threw, "periodic task should reject once shutdown starts");
}

void test_threadpool_periodic_stops_after_request_stop() {
  rtl::stp::ThreadPool tp(2, 4, 2);
  std::atomic<int> counter{0};

  tp.put_periodic(
      10, [&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });

  std::this_thread::sleep_for(60ms);
  tp.request_stop();
  const int before_wait = counter.load(std::memory_order_relaxed);
  std::this_thread::sleep_for(40ms);
  const int after_wait = counter.load(std::memory_order_relaxed);

  require(after_wait == before_wait,
          "periodic task continued running after shutdown");
}

void test_executor_one_shot_submit_success() {
  auto executor = rtl::stp::makeThreadPoolExecutor(2, 4);
  require(static_cast<bool>(executor), "makeThreadPoolExecutor returned nullptr");

  rtl::stp::TaskOptions options{.is_periodic = false,
                                .periodic_interval_ms = 0};
  auto future = executor->submit(options, [](int a, int b) { return a + b; },
                                 10, 32);

  require(future.get() == 42, "executor one-shot submit returned wrong result");
}

void test_executor_one_shot_rejection_future() {
  RejectingExecutor executor;
  rtl::stp::TaskOptions options{.is_periodic = false,
                                .periodic_interval_ms = 0};
  auto future = executor.submit(options, []() { return 7; });

  bool caught = false;
  try {
    (void)future.get();
  } catch (const rtl::stp::TaskRejected &exp) {
    caught = exp.code() == rtl::stp::ErrorCode::task_rejected;
  }

  require(caught, "executor one-shot rejection was not surfaced by future");
}

void test_executor_periodic_submit_success_returns_invalid_future() {
  auto executor = rtl::stp::makeThreadPoolExecutor(2, 4);
  require(static_cast<bool>(executor), "makeThreadPoolExecutor returned nullptr");

  std::atomic<int> counter{0};
  rtl::stp::TaskOptions options{.is_periodic = true,
                                .periodic_interval_ms = 10};
  auto future = executor->submit(options, [&counter]() {
    counter.fetch_add(1, std::memory_order_relaxed);
  });

  require(!future.valid(),
          "accepted periodic submit should return invalid fire-and-forget future");
  std::this_thread::sleep_for(40ms);
  require(counter.load(std::memory_order_relaxed) > 0,
          "accepted periodic submit did not execute task");
}

void test_executor_periodic_rejection_future() {
  RejectingExecutor executor;
  rtl::stp::TaskOptions options{.is_periodic = true,
                                .periodic_interval_ms = 10};
  auto future = executor.submit(options, []() {});

  require(future.valid(),
          "rejected periodic submit should return a future carrying rejection");

  bool caught = false;
  try {
    future.get();
  } catch (const rtl::stp::TaskRejected &exp) {
    caught = exp.code() == rtl::stp::ErrorCode::task_rejected;
  }

  require(caught, "executor periodic rejection was not surfaced by future");
}

void run_test(void (*test)(), const char *name) {
  test();
  std::cout << "[PASS] " << name << '\n';
}

} // namespace

int main() {
  try {
    run_test(test_threadpool_basic, "threadpool_basic");
    run_test(test_threadpool_many_tasks, "threadpool_many_tasks");
    run_test(test_threadpool_exception_propagation,
             "threadpool_exception_propagation");
    run_test(test_threadpool_destructor_drains_accepted_tasks,
             "threadpool_destructor_drains_accepted_tasks");
    run_test(test_threadpool_rejects_put_after_stop,
             "threadpool_rejects_put_after_stop");
    run_test(test_threadpool_rejects_periodic_after_stop,
             "threadpool_rejects_periodic_after_stop");
    run_test(test_threadpool_periodic_stops_after_request_stop,
             "threadpool_periodic_stops_after_request_stop");
    run_test(test_executor_one_shot_submit_success,
             "executor_one_shot_submit_success");
    run_test(test_executor_one_shot_rejection_future,
             "executor_one_shot_rejection_future");
    run_test(test_executor_periodic_submit_success_returns_invalid_future,
             "executor_periodic_submit_success_returns_invalid_future");
    run_test(test_executor_periodic_rejection_future,
             "executor_periodic_rejection_future");
  } catch (const std::exception &exp) {
    std::cerr << "[FAIL] " << exp.what() << '\n';
    return 1;
  }

  return 0;
}
