#pragma once

#include <condition_variable>
#include <exception>
#include <mutex>
#include <optional>

#include "CoroExceptions.hpp"
#include "Task.hpp"


namespace rtl::coro {


namespace detail {
template <typename T>
struct SyncWaitState {
  std::mutex mtx;
  std::condition_variable cv;
  bool done{false};
  std::optional<T> result;
  std::exception_ptr exception;
};


template <>
struct SyncWaitState<void> {
  std::mutex mtx;
  std::condition_variable cv;
  bool done{false};
  std::exception_ptr exception;
};


template <typename T>
Task<void> sync_wait_runner(Task<T> task, SyncWaitState<T>& state) {
  try {
    auto value = co_await std::move(task);
    {
      std::lock_guard lock{state.mtx};
      state.result.emplace(std::move(value));
      state.done = true;
    }
  } catch (...) {
    {
      std::lock_guard lock{state.mtx};
      state.exception = std::current_exception();
      state.done = true;
    }
  }
  state.cv.notify_one();
  co_return;
};


inline Task<void> sync_wait_runner(Task<void> task, SyncWaitState<void>& state) {
  try {
    co_await std::move(task);
    {
      std::lock_guard lock{state.mtx};
      state.done = true;
    }
  } catch (...) {
    {
      std::lock_guard lock{state.mtx};
      state.exception = std::current_exception();
      state.done = true;
    }
  }
  state.cv.notify_one();
  co_return;
};

}; // namespace detail

template <typename T>
T sync_wait(Task<T> task) {
  detail::SyncWaitState<T> state;
  auto runner = detail::sync_wait_runner(std::move(task), state);
  runner.resume();
  std::unique_lock lock{state.mtx};
  state.cv.wait(lock, [&]() { return state.done; });
  if (state.exception) {
    std::rethrow_exception(state.exception);
  }
  if (!state.result.has_value()) {
    throw EmptyResult{"Task completed without result"};
  }
  return std::move(*state.result);
};


inline void sync_wait(Task<void> task) {
  detail::SyncWaitState<void> state;
  auto runner = detail::sync_wait_runner(std::move(task), state);
  runner.resume();
  std::unique_lock lock{state.mtx};
  state.cv.wait(lock, [&]() { return state.done; });
  if (state.exception) {
    std::rethrow_exception(state.exception);
  }
  return;
};

}; // namespace rtl::coro