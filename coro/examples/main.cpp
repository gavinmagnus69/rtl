#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <SyncWait.hpp>

namespace {

rtl::coro::Task<int> read_value() {
  co_return 40;
}

rtl::coro::Task<void> prepare_value() {
  co_return;
}

rtl::coro::Task<int> compute_value() {
  co_await prepare_value();
  auto base = read_value();
  const int value = co_await std::move(base);
  co_return value + 2;
}

rtl::coro::Task<std::unique_ptr<int>> make_unique_value() {
  co_return std::make_unique<int>(7);
}

rtl::coro::Task<int> fail_value() {
  throw std::runtime_error{"example failure"};
  co_return 0;
}

} // namespace

int main() {
  try {
    const int value = rtl::coro::sync_wait(compute_value());
    std::cout << "computed value: " << value << '\n';

    auto uniqueValue = rtl::coro::sync_wait(make_unique_value());
    std::cout << "move-only value: " << *uniqueValue << '\n';

    try {
      (void)rtl::coro::sync_wait(fail_value());
    } catch (const std::exception& exp) {
      std::cout << "caught coroutine exception: " << exp.what() << '\n';
    }
  } catch (const std::exception& exp) {
    std::cerr << "coro example failed: " << exp.what() << '\n';
    return 1;
  }

  return 0;
}
