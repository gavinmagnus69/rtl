#include "Connection.hpp"
#include "ConnectionPool.hpp"
#include "Errors.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

using db::core::Connection;
using db::core::ConnectionConfig;
using db::core::PoolTimeout;
using db::pool::ConnectionPool;
using db::pool::PoolConfig;

constexpr std::string_view kReuseTempTableName = "codex_db_pool_reuse_probe";
constexpr std::string_view kBrokenTempTableName = "codex_db_pool_broken_probe";

std::string env_or(const char *name, std::string default_value) {
  if (const char *value = std::getenv(name);
      value != nullptr && *value != '\0') {
    return value;
  }
  return default_value;
}

std::uint16_t env_port_or(const char *name, std::uint16_t default_value) {
  if (const char *value = std::getenv(name);
      value != nullptr && *value != '\0') {
    const auto parsed = std::stoi(value);
    if (parsed <= 0 || parsed > 65535) {
      throw std::runtime_error("DB_TEST_PORT must be in range 1..65535");
    }
    return static_cast<std::uint16_t>(parsed);
  }
  return default_value;
}

ConnectionConfig make_test_config() {
  ConnectionConfig config;
  config.host = env_or("DB_TEST_HOST", "127.0.0.1");
  config.port = env_port_or("DB_TEST_PORT", 5432);
  config.dbname = env_or("DB_TEST_DBNAME", "postgres");
  config.user = env_or("DB_TEST_USER", "postgres");
  config.password = env_or("DB_TEST_PASSWORD", "postgres");
  config.application_name = "libsdbpool_integration";
  return config;
}

PoolConfig make_single_connection_pool_config() {
  PoolConfig config;
  config.max_connections = 1;
  return config;
}

void expect(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

bool temp_table_exists(Connection &connection, std::string_view table_name) {
  const auto result =
      connection.exec("select to_regclass('pg_temp." + std::string(table_name) +
                      "') is not null as present");
  return result[0]["present"].as_bool();
}

void create_temp_table(Connection &connection, std::string_view table_name) {
  static_cast<void>(connection.exec("create temp table " +
                                    std::string(table_name) +
                                    " (id bigint primary key)"));
}

void test_acquire_release_reuses_session(
    const ConnectionConfig &connection_config) {
  ConnectionPool pool{make_single_connection_pool_config(), connection_config};

  expect(pool.idle_count() == 0,
         "lazy pool should start with zero idle connections");
  expect(pool.active_count() == 0,
         "lazy pool should start with zero active connections");

  {
    auto lease = pool.acquire();
    expect(lease.valid(), "acquire() should return a valid pooled connection");
    expect(pool.active_count() == 1,
           "checked-out lease should increase active count");
    create_temp_table(*lease, kReuseTempTableName);
    expect(temp_table_exists(*lease, kReuseTempTableName),
           "temp table should exist in the checked-out session");
  }

  expect(pool.active_count() == 0,
         "released lease should decrement active count");
  expect(pool.idle_count() == 1,
         "healthy released connection should become idle");

  {
    auto lease = pool.acquire();
    expect(temp_table_exists(*lease, kReuseTempTableName),
           "reacquiring from a size-1 pool should reuse the same session");
  }
}

void test_try_acquire_for_times_out(const ConnectionConfig &connection_config) {
  ConnectionPool pool{make_single_connection_pool_config(), connection_config};
  auto held = pool.acquire();

  std::promise<void> started;
  auto started_future = started.get_future();
  auto timeout_future = std::async(std::launch::async, [&]() {
    started.set_value();
    try {
      auto waiting = pool.try_acquire_for(150);
      static_cast<void>(waiting);
      return false;
    } catch (const PoolTimeout &) {
      return true;
    }
  });

  started_future.wait();
  expect(timeout_future.wait_for(1s) == std::future_status::ready,
         "timed acquire should finish once the timeout expires");
  expect(timeout_future.get(), "timed acquire should throw PoolTimeout");
  expect(pool.active_count() == 1,
         "timed-out waiter must not change active connection count");
  static_cast<void>(held);
}

void test_pool_blocks_when_max_size_is_reached(
    const ConnectionConfig &connection_config) {
  ConnectionPool pool{make_single_connection_pool_config(), connection_config};
  std::future<bool> waiter;

  {
    auto held = pool.acquire();

    std::promise<void> started;
    auto started_future = started.get_future();
    waiter = std::async(std::launch::async, [&]() {
      started.set_value();
      auto acquired = pool.acquire();
      return acquired.valid();
    });

    started_future.wait();
    std::this_thread::sleep_for(150ms);
    expect(pool.active_count() == 1,
           "pool must not hand out more than max_connections active leases");
    expect(waiter.wait_for(0ms) == std::future_status::timeout,
           "second acquire() should block while the only connection is checked "
           "out");
    static_cast<void>(held);
  }

  expect(waiter.wait_for(1s) == std::future_status::ready,
         "blocked acquire should finish after a connection is released");
  expect(waiter.get(), "blocked acquire should succeed after release");
  expect(pool.idle_count() == 1,
         "pool should end with one reusable idle connection");
}

void test_broken_connection_is_evicted(
    const ConnectionConfig &connection_config) {
  ConnectionPool pool{make_single_connection_pool_config(), connection_config};

  {
    auto lease = pool.acquire();
    create_temp_table(*lease, kBrokenTempTableName);
    expect(temp_table_exists(*lease, kBrokenTempTableName),
           "temp table should exist before marking the connection broken");
    lease.mark_broken();
  }

  expect(pool.active_count() == 0,
         "releasing a broken connection should decrement active count");
  expect(pool.idle_count() == 0,
         "broken connection should be discarded instead of returned idle");

  {
    auto lease = pool.acquire();
    expect(!temp_table_exists(*lease, kBrokenTempTableName),
           "reacquiring after broken release should create a fresh DB session");
  }
}

struct TestCase {
  std::string_view name;
  std::function<void()> run;
};

// namespace

int main() {
  try {
    const ConnectionConfig connection_config = make_test_config();
    const std::vector<TestCase> tests{
        {"acquire/release reuse",
         [&]() { test_acquire_release_reuses_session(connection_config); }},
        {"timed acquire timeout",
         [&]() { test_try_acquire_for_times_out(connection_config); }},
        {"max-size enforcement",
         [&]() {
           test_pool_blocks_when_max_size_is_reached(connection_config);
         }},
        {"broken connection eviction",
         [&]() { test_broken_connection_is_evicted(connection_config); }},
    };

    std::size_t passed = 0;
    for (const auto &test : tests) {
      test.run();
      ++passed;
      std::cout << "[PASS] " << test.name << '\n';
    }

    std::cout << "Passed " << passed << " pool integration tests." << '\n';
    return 0;
  } catch (const std::exception &ex) {
    std::cerr << "[FAIL] " << ex.what() << '\n';
    std::cerr << "You can override connection settings with DB_TEST_HOST, "
                 "DB_TEST_PORT, DB_TEST_DBNAME, DB_TEST_USER, and "
                 "DB_TEST_PASSWORD."
              << '\n';
    return 1;
  }
}
