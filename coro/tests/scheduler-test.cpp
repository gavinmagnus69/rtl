#include <coroutine>
#include <cstddef>
#include <deque>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <CoroExceptions.hpp>
#include <ExecutorScheduler.hpp>
#include <Schedule.hpp>
#include <Task.hpp>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

class FakeExecutor final : public rtl::stp::IExecutor {
public:
  void reject_posts(bool reject) noexcept {
    m_rejectPosts = reject;
  }

  [[nodiscard]] std::size_t pending_count() const noexcept {
    return m_tasks.size();
  }

  bool run_one() {
    if (m_tasks.empty()) {
      return false;
    }

    auto task = std::move(m_tasks.front());
    m_tasks.pop_front();
    task();
    return true;
  }

protected:
  bool post(rtl::stp::Task&& task, rtl::stp::TaskOptions) noexcept final {
    if (m_rejectPosts || !task) {
      return false;
    }

    m_tasks.emplace_back(std::move(task));
    return true;
  }

private:
  bool m_rejectPosts{false};
  std::deque<rtl::stp::Task> m_tasks;
};

rtl::coro::Task<void> scheduled_void(rtl::coro::ExecutorScheduler& scheduler, int& state) {
  state = 1;
  co_await rtl::coro::schedule(scheduler);
  state = 2;
  co_return;
}

rtl::coro::Task<void> resume_on_void(rtl::coro::ExecutorScheduler& scheduler, int& state) {
  state = 1;
  co_await rtl::coro::resume_on(scheduler);
  state = 2;
  co_return;
}

rtl::coro::Task<void> catches_schedule_rejection(rtl::coro::ExecutorScheduler& scheduler, bool& caught) {
  try {
    co_await rtl::coro::schedule(scheduler);
  } catch (const rtl::coro::ScheduleRejected&) {
    caught = true;
  }
  co_return;
}

rtl::coro::Task<void> throws_after_schedule(rtl::coro::ExecutorScheduler& scheduler) {
  co_await rtl::coro::schedule(scheduler);
  throw std::runtime_error{"scheduled user failure"};
  co_return;
}

void test_post_rejects_null_executor() {
  rtl::coro::ExecutorScheduler scheduler{nullptr};

  require(!scheduler.post(std::noop_coroutine()), "scheduler accepted post with null executor");
}

void test_post_rejects_null_handle() {
  auto executor = std::make_shared<FakeExecutor>();
  rtl::coro::ExecutorScheduler scheduler{executor};

  require(!scheduler.post(std::coroutine_handle<>{}), "scheduler accepted null coroutine handle");
  require(executor->pending_count() == 0, "null coroutine handle was queued");
}

void test_post_queues_and_resumes_on_run_one() {
  auto executor = std::make_shared<FakeExecutor>();
  rtl::coro::ExecutorScheduler scheduler{executor};
  int state = 0;

  auto task = scheduled_void(scheduler, state);
  task.resume();

  require(state == 1, "task did not run until schedule point");
  require(executor->pending_count() == 1, "scheduled continuation was not queued");
  require(!task.done(), "task completed before queued continuation ran");

  require(executor->run_one(), "queued continuation did not run");
  require(state == 2, "task did not resume through executor");
  require(task.done(), "task did not complete after executor resume");
  task.result();
}

void test_schedule_awaiter_suspends_and_resumes() {
  auto executor = std::make_shared<FakeExecutor>();
  rtl::coro::ExecutorScheduler scheduler{executor};
  int state = 0;

  auto task = scheduled_void(scheduler, state);
  task.resume();

  require(state == 1, "schedule did not suspend before continuation body");
  require(executor->run_one(), "schedule continuation was not queued");
  require(state == 2, "schedule continuation did not resume");
  task.result();
}

void test_resume_on_matches_schedule() {
  auto executor = std::make_shared<FakeExecutor>();
  rtl::coro::ExecutorScheduler scheduler{executor};
  int state = 0;

  auto task = resume_on_void(scheduler, state);
  task.resume();

  require(state == 1, "resume_on did not suspend before continuation body");
  require(executor->pending_count() == 1, "resume_on continuation was not queued");
  require(executor->run_one(), "resume_on continuation did not run");
  require(state == 2, "resume_on continuation did not resume");
  task.result();
}

void test_schedule_rejection_reaches_coroutine() {
  auto executor = std::make_shared<FakeExecutor>();
  executor->reject_posts(true);
  rtl::coro::ExecutorScheduler scheduler{executor};
  bool caught = false;

  auto task = catches_schedule_rejection(scheduler, caught);
  task.resume();

  require(caught, "ScheduleRejected did not reach coroutine");
  require(task.done(), "task did not complete after schedule rejection");
  require(executor->pending_count() == 0, "rejected schedule queued continuation");
  task.result();
}

void test_user_exception_after_schedule_propagates() {
  auto executor = std::make_shared<FakeExecutor>();
  rtl::coro::ExecutorScheduler scheduler{executor};

  auto task = throws_after_schedule(scheduler);
  task.resume();
  require(executor->run_one(), "scheduled throwing continuation did not run");

  bool caught = false;
  try {
    task.result();
  } catch (const std::runtime_error& exp) {
    caught = std::string{exp.what()} == "scheduled user failure";
  }

  require(caught, "scheduled user exception did not propagate unchanged");
}

} // namespace

int main() {
  try {
    test_post_rejects_null_executor();
    test_post_rejects_null_handle();
    test_post_queues_and_resumes_on_run_one();
    test_schedule_awaiter_suspends_and_resumes();
    test_resume_on_matches_schedule();
    test_schedule_rejection_reaches_coroutine();
    test_user_exception_after_schedule_propagates();
  } catch (const std::exception& exp) {
    std::cerr << "coro scheduler test failed: " << exp.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "coro scheduler test failed with unknown exception\n";
    return 1;
  }

  std::cout << "coro scheduler test passed successfully\n";
  return 0;
}
