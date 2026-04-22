#include "Connection.hpp"
#include "ConnectionPool.hpp"
#include "Errors.hpp"
#include "Savepoint.hpp"
#include "Transaction.hpp"
#include "TransactionHelper.hpp"

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

namespace {

using db::core::ConnectionConfig;
using db::core::DeadlockDetected;
using db::core::DbParam;
using db::core::QueryError;
using db::core::Result;
using db::core::SerializationFailure;
using db::pool::ConnectionPool;
using db::pool::PoolConfig;
using db::tx::IsolationLevel;
using db::tx::Savepoint;
using db::tx::Transaction;
using db::tx::TransactionHelper;

constexpr std::string_view kTempTableName = "codex_db_tx_integration_users";
constexpr std::string_view kPreparedName = "codex_db_tx_select_prepared";

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
  config.application_name = "libsdbtx_integration";
  return config;
}

PoolConfig make_single_connection_pool_config() {
  PoolConfig config;
  config.max_connections = 1;
  return config;
}

ConnectionPool make_pool() {
  return ConnectionPool{make_single_connection_pool_config(),
                        make_test_config()};
}

void expect(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename Exception, typename Fn>
void expect_throws(std::string_view name, Fn &&fn) {
  try {
    std::forward<Fn>(fn)();
  } catch (const Exception &) {
    return;
  } catch (const std::exception &ex) {
    throw std::runtime_error(std::string(name) +
                             " threw unexpected exception: " + ex.what());
  }

  throw std::runtime_error(std::string(name) + " did not throw");
}

void create_temp_table(ConnectionPool &pool) {
  auto lease = pool.acquire();
  static_cast<void>(lease->exec("create temp table if not exists " +
                                std::string(kTempTableName) +
                                " ("
                                "  id bigint primary key,"
                                "  email text not null"
                                ")"));
}

void truncate_temp_table(ConnectionPool &pool) {
  auto lease = pool.acquire();
  static_cast<void>(
      lease->exec("truncate table " + std::string(kTempTableName)));
}

std::int64_t count_rows(ConnectionPool &pool) {
  auto lease = pool.acquire();
  const Result result = lease->exec("select count(*) as count from " +
                                    std::string(kTempTableName));
  return result[0]["count"].as_int64();
}

std::string current_isolation_level(Transaction &tx) {
  const Result result = tx.exec(
      "select current_setting('transaction_isolation') as isolation_level");
  return result[0]["isolation_level"].as_string();
}

void prepare_statement(ConnectionPool &pool) {
  auto lease = pool.acquire();
  lease->prepare(std::string(kPreparedName),
                 "select $1::bigint as id, $2::text as email");
}

void insert_user(Transaction &tx, std::int64_t id, std::string email) {
  static_cast<void>(
      tx.exec_params("insert into " + std::string(kTempTableName) +
                         "(id, email) values ($1, $2)",
                     {DbParam{"id", id}, DbParam{"email", std::move(email)}}));
}

void test_transaction_is_active_and_exec_works() {
  auto pool = make_pool();
  Transaction tx(pool);

  expect(tx.active(), "transaction should be active after construction");

  const Result result = tx.exec("select 1 as value");
  expect(result.valid(), "exec() result should be valid");
  expect(result.size() == 1, "exec() should return one row");
  expect(result[0]["value"].as_int64() == 1,
         "exec() should return selected value");

  tx.commit();
  expect(!tx.active(), "transaction should become inactive after commit");
}

void test_exec_params_and_commit_persist_changes() {
  auto pool = make_pool();
  create_temp_table(pool);
  truncate_temp_table(pool);

  {
    Transaction tx(pool);
    insert_user(tx, 1, "commit@example.com");
    tx.commit();
    expect(!tx.active(), "commit should deactivate the transaction");
  }

  expect(count_rows(pool) == 1,
         "committed transaction should persist inserted row");
}

void test_exec_prepared_uses_connection_prepared_statement() {
  auto pool = make_pool();
  prepare_statement(pool);

  Transaction tx(pool);
  const Result result = tx.exec_prepared(
      kPreparedName, {DbParam{"id", std::int64_t{7}},
                      DbParam{"email", std::string{"prepared@example.com"}}});

  expect(result.size() == 1, "exec_prepared should return one row");
  expect(result[0]["id"].as_int64() == 7, "prepared bigint param should bind");
  expect(result[0]["email"].as_string() == "prepared@example.com",
         "prepared text param should bind");
  tx.commit();
}

void test_rollback_discards_changes() {
  auto pool = make_pool();
  create_temp_table(pool);
  truncate_temp_table(pool);

  {
    Transaction tx(pool);
    insert_user(tx, 1, "rollback@example.com");
    tx.rollback();
    expect(!tx.active(), "rollback should deactivate the transaction");
  }

  expect(count_rows(pool) == 0,
         "rolled-back transaction should not persist rows");
}

void test_destructor_rolls_back_active_transaction() {
  auto pool = make_pool();
  create_temp_table(pool);
  truncate_temp_table(pool);

  {
    Transaction tx(pool);
    insert_user(tx, 1, "destructor@example.com");
    expect(tx.active(), "transaction should remain active before scope exit");
  }

  expect(count_rows(pool) == 0,
         "destroying an active transaction should roll back changes");
}

void test_move_constructor_preserves_transaction() {
  auto pool = make_pool();
  create_temp_table(pool);
  truncate_temp_table(pool);

  Transaction moved_to = [&]() {
    Transaction tx(pool);
    insert_user(tx, 1, "move@example.com");
    return Transaction(std::move(tx));
  }();

  expect(moved_to.active(), "moved transaction should remain active");
  moved_to.commit();
  expect(count_rows(pool) == 1, "moved transaction should stay usable");
}

void test_invalid_sql_maps_to_query_error() {
  auto pool = make_pool();
  Transaction tx(pool);

  expect_throws<QueryError>(
      "invalid SQL through tx should map to QueryError",
      [&]() { static_cast<void>(tx.exec("select from")); });
}

void test_default_isolation_level_is_read_committed() {
  auto pool = make_pool();
  Transaction tx(pool);

  expect(current_isolation_level(tx) == "read committed",
         "default transaction should use READ COMMITTED isolation");
  tx.commit();
}

void test_repeatable_read_isolation_level_is_applied() {
  auto pool = make_pool();
  Transaction tx(pool, IsolationLevel::repeatable_read);

  expect(current_isolation_level(tx) == "repeatable read",
         "transaction should apply REPEATABLE READ isolation");
  tx.commit();
}

void test_serializable_isolation_level_is_applied() {
  auto pool = make_pool();
  Transaction tx(pool, IsolationLevel::serializable);

  expect(current_isolation_level(tx) == "serializable",
         "transaction should apply SERIALIZABLE isolation");
  tx.commit();
}

void test_savepoint_rollback_discards_only_changes_after_savepoint() {
  auto pool = make_pool();
  create_temp_table(pool);
  truncate_temp_table(pool);

  Transaction tx(pool);
  insert_user(tx, 1, "before-savepoint@example.com");

  Savepoint sp(tx);
  expect(sp.active(), "savepoint should be active after construction");
  expect(!sp.name().empty(), "savepoint should expose a generated name");

  insert_user(tx, 2, "after-savepoint@example.com");
  sp.rollback();

  expect(!sp.active(), "rollback should deactivate the savepoint");
  expect(tx.active(), "outer transaction should stay active after rollback");

  tx.commit();
  expect(count_rows(pool) == 1,
         "rollback to savepoint should preserve earlier changes only");
}

void test_savepoint_release_keeps_changes_after_savepoint() {
  auto pool = make_pool();
  create_temp_table(pool);
  truncate_temp_table(pool);

  Transaction tx(pool);
  insert_user(tx, 1, "before-release@example.com");

  Savepoint sp(tx);
  insert_user(tx, 2, "after-release@example.com");
  sp.release();

  expect(!sp.active(), "release should deactivate the savepoint");
  expect(tx.active(), "outer transaction should stay active after release");

  tx.commit();
  expect(count_rows(pool) == 2,
         "released savepoint should keep changes after the savepoint");
}

void test_savepoint_destructor_rolls_back_to_savepoint() {
  auto pool = make_pool();
  create_temp_table(pool);
  truncate_temp_table(pool);

  Transaction tx(pool);
  insert_user(tx, 1, "before-savepoint-destructor@example.com");

  {
    Savepoint sp(tx);
    insert_user(tx, 2, "rolled-back-by-savepoint-destructor@example.com");
    expect(sp.active(), "savepoint should remain active before scope exit");
  }

  expect(tx.active(),
         "outer transaction should stay active after savepoint destruction");
  tx.commit();
  expect(count_rows(pool) == 1,
         "destroying an active savepoint should roll back only nested changes");
}

void test_savepoint_requires_active_transaction() {
  auto pool = make_pool();
  Transaction tx(pool);
  tx.commit();

  expect_throws<std::logic_error>(
      "savepoint construction should fail for inactive transaction",
      [&] { Savepoint sp(tx); });
}

void test_retry_helper_retries_serialization_failure_and_returns_value() {
  auto pool = make_pool();
  create_temp_table(pool);
  truncate_temp_table(pool);

  TransactionHelper helper;
  helper.m_maxRetries = 3;

  int attempts = 0;
  const int result = helper.with_retry(
      pool, IsolationLevel::serializable, [&](Transaction &tx) -> int {
        ++attempts;
        insert_user(tx, 1, "serialization-retry@example.com");
        if (attempts == 1) {
          throw SerializationFailure{"retry serialization failure"};
        }
        return 42;
      });

  expect(attempts == 2,
         "serialization failure should retry the transaction body once");
  expect(result == 42, "retry helper should return callback result");
  expect(count_rows(pool) == 1,
         "successful retry should commit only the final attempt");
}

void test_retry_helper_retries_deadlock_and_supports_void_callback() {
  auto pool = make_pool();
  create_temp_table(pool);
  truncate_temp_table(pool);

  TransactionHelper helper;
  helper.m_maxRetries = 3;

  int attempts = 0;
  helper.with_retry(pool, IsolationLevel::serializable, [&](Transaction &tx) {
    ++attempts;
    insert_user(tx, 1, "deadlock-retry@example.com");
    if (attempts == 1) {
      throw DeadlockDetected{"retry deadlock detected"};
    }
  });

  expect(attempts == 2, "deadlock should retry the transaction body once");
  expect(count_rows(pool) == 1,
         "successful deadlock retry should commit only one row");
}

void test_retry_helper_rethrows_after_retry_limit() {
  auto pool = make_pool();
  create_temp_table(pool);
  truncate_temp_table(pool);

  TransactionHelper helper;
  helper.m_maxRetries = 2;

  int attempts = 0;
  expect_throws<SerializationFailure>(
      "retry helper should rethrow serialization failure after max retries",
      [&] {
        helper.with_retry(pool, IsolationLevel::serializable,
                          [&](Transaction &tx) {
                            ++attempts;
                            insert_user(tx, attempts,
                                        "serialization-fail@example.com");
                            throw SerializationFailure{
                                "always fail with serialization failure"};
                          });
      });

  expect(attempts == 2,
         "retry helper should stop after the configured retry limit");
  expect(count_rows(pool) == 0,
         "exhausted retries should leave no committed rows");
}

void test_retry_helper_does_not_retry_non_retryable_errors() {
  auto pool = make_pool();
  create_temp_table(pool);
  truncate_temp_table(pool);

  TransactionHelper helper;
  helper.m_maxRetries = 3;

  int attempts = 0;
  expect_throws<QueryError>(
      "retry helper should not retry non-retryable query errors", [&] {
        helper.with_retry(pool, IsolationLevel::serializable,
                          [&](Transaction &tx) {
                            ++attempts;
                            insert_user(tx, 1, "query-error@example.com");
                            throw QueryError{"non-retryable query error"};
                          });
      });

  expect(attempts == 1,
         "non-retryable exceptions should escape on the first attempt");
  expect(count_rows(pool) == 0,
         "non-retryable failure should not leave committed rows");
}

void test_retry_helper_rejects_zero_retry_limit() {
  auto pool = make_pool();
  TransactionHelper helper;
  helper.m_maxRetries = 0;

  expect_throws<std::logic_error>(
      "retry helper should reject zero retries",
      [&] { helper.with_retry(pool, IsolationLevel::serializable,
                              [](Transaction &) {}); });
}

struct TestCase {
  std::string_view name;
  std::function<void()> run;
};

} // namespace

int main() {
  try {
    const std::vector<TestCase> tests{
        {"active + exec", test_transaction_is_active_and_exec_works},
        {"exec_params + commit", test_exec_params_and_commit_persist_changes},
        {"exec_prepared",
         test_exec_prepared_uses_connection_prepared_statement},
        {"rollback", test_rollback_discards_changes},
        {"destructor rollback", test_destructor_rolls_back_active_transaction},
        {"move constructor", test_move_constructor_preserves_transaction},
        {"invalid sql -> query error", test_invalid_sql_maps_to_query_error},
        {"default isolation -> read committed",
         test_default_isolation_level_is_read_committed},
        {"repeatable read isolation",
         test_repeatable_read_isolation_level_is_applied},
        {"serializable isolation",
         test_serializable_isolation_level_is_applied},
        {"savepoint rollback -> preserve earlier changes",
         test_savepoint_rollback_discards_only_changes_after_savepoint},
        {"savepoint release -> keep nested changes",
         test_savepoint_release_keeps_changes_after_savepoint},
        {"savepoint destructor -> nested rollback",
         test_savepoint_destructor_rolls_back_to_savepoint},
        {"savepoint requires active transaction",
         test_savepoint_requires_active_transaction},
        {"retry helper -> serialization retry + value result",
         test_retry_helper_retries_serialization_failure_and_returns_value},
        {"retry helper -> deadlock retry + void callback",
         test_retry_helper_retries_deadlock_and_supports_void_callback},
        {"retry helper -> max retries rethrow",
         test_retry_helper_rethrows_after_retry_limit},
        {"retry helper -> non-retryable passthrough",
         test_retry_helper_does_not_retry_non_retryable_errors},
        {"retry helper -> zero retries rejected",
         test_retry_helper_rejects_zero_retry_limit},
    };

    std::size_t passed = 0;
    for (const auto &test : tests) {
      SPDLOG_INFO("Test: {}", test.name);
      test.run();
      ++passed;
      SPDLOG_INFO("[PASS] {}", test.name);
      // std::cout << "[PASS] " << test.name << '\n';
    }
    SPDLOG_INFO("Passed {} tx integration tests.", passed);
    // std::cout << "Passed " << passed << " tx integration tests." << '\n';
    return 0;
  } catch (const std::exception &ex) {
    SPDLOG_ERROR("[FAIL] {}", ex.what());
    SPDLOG_ERROR("You can override connection settings with DB_TEST_HOST, "
                 "DB_TEST_PORT, DB_TEST_DBNAME, DB_TEST_USER, and "
                 "DB_TEST_PASSWORD.");

    // std::cerr << "[FAIL] " << ex.what() << '\n';
    // std::cerr << "You can override connection settings with DB_TEST_HOST, "
    //              "DB_TEST_PORT, DB_TEST_DBNAME, DB_TEST_USER, and "
    //              "DB_TEST_PASSWORD."
    //           << '\n';
    return 1;
  }
}
