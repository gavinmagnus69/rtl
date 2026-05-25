#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include <SyncWait.hpp>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

rtl::coro::Task<int> return_value() {
  co_return 42;
}

rtl::coro::Task<void> return_void() {
  co_return;
}

rtl::coro::Task<int> throw_value_exception() {
  throw std::runtime_error("value failure");
  co_return 0;
}

rtl::coro::Task<void> throw_void_exception() {
  throw std::runtime_error("void failure");
  co_return;
}

rtl::coro::Task<int> child_value() {
  co_return 40;
}

rtl::coro::Task<int> nested_value() {
  auto child = child_value();
  const int value = co_await std::move(child);
  co_return value + 2;
}

rtl::coro::Task<void> child_void() {
  co_return;
}

rtl::coro::Task<int> nested_void() {
  auto child = child_void();
  co_await std::move(child);
  co_return 7;
}

rtl::coro::Task<std::unique_ptr<int>> move_only_result() {
  co_return std::make_unique<int>(11);
}

void test_sync_wait_returns_value() {
  const int value = rtl::coro::sync_wait(return_value());
  require(value == 42, "sync_wait(Task<int>) returned wrong value");
}

void test_sync_wait_void_completes() {
  rtl::coro::sync_wait(return_void());
}

void test_sync_wait_rethrows_value_exception() {
  bool caught = false;
  try {
    (void)rtl::coro::sync_wait(throw_value_exception());
  } catch (const std::runtime_error& exp) {
    caught = std::string(exp.what()) == "value failure";
  }

  require(caught, "sync_wait(Task<T>) did not rethrow task exception");
}

void test_sync_wait_rethrows_void_exception() {
  bool caught = false;
  try {
    rtl::coro::sync_wait(throw_void_exception());
  } catch (const std::runtime_error& exp) {
    caught = std::string(exp.what()) == "void failure";
  }

  require(caught, "sync_wait(Task<void>) did not rethrow task exception");
}

void test_nested_value_await() {
  const int value = rtl::coro::sync_wait(nested_value());
  require(value == 42, "nested Task<int> co_await returned wrong value");
}

void test_nested_void_await() {
  const int value = rtl::coro::sync_wait(nested_void());
  require(value == 7, "nested Task<void> co_await did not resume parent");
}

void test_move_only_result() {
  auto value = rtl::coro::sync_wait(move_only_result());
  require(value != nullptr, "move-only result was null");
  require(*value == 11, "move-only result had wrong value");
}

void test_destroy_unawaited_tasks() {
  auto value_task = return_value();
  auto void_task = return_void();
}

void test_move_transfers_task_ownership() {
  auto task = return_value();
  auto moved = std::move(task);
  const int value = rtl::coro::sync_wait(std::move(moved));
  require(value == 42, "moved Task<int> returned wrong value");
}

} // namespace

int main() {
  try {
    test_sync_wait_returns_value();
    test_sync_wait_void_completes();
    test_sync_wait_rethrows_value_exception();
    test_sync_wait_rethrows_void_exception();
    test_nested_value_await();
    test_nested_void_await();
    test_move_only_result();
    test_destroy_unawaited_tasks();
    test_move_transfers_task_ownership();
  } catch (const std::exception& exp) {
    std::cerr << "coro task test failed: " << exp.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "coro task test failed with unknown exception\n";
    return 1;
  }
  std::cout << "coro test passed successfully\n";
  return 0;
}
