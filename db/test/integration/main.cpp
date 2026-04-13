#include "Connection.hpp"
#include "Errors.hpp"

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using db::core::Connection;
using db::core::ConnectionConfig;
using db::core::ConstraintViolation;
using db::core::DbParam;
using db::core::MappingError;
using db::core::QueryError;
using db::core::Result;

constexpr std::string_view kTempTableName = "codex_db_core_integration_users";

std::string table_name() { return std::string(kTempTableName); }

std::string insert_user_sql() {
  return "insert into " + table_name() + "(id, email, active) values ($1, $2, $3)";
}

std::string env_or(const char *name, std::string default_value) {
  if (const char *value = std::getenv(name); value != nullptr && *value != '\0') {
    return value;
  }
  return default_value;
}

std::uint16_t env_port_or(const char *name, std::uint16_t default_value) {
  if (const char *value = std::getenv(name); value != nullptr && *value != '\0') {
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
  config.application_name = "libsdbcore_integration";
  return config;
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

void create_temp_schema(Connection &connection) {
  connection.exec(std::string("create temp table ") + table_name() + " ("
                  "  id bigint primary key,"
                  "  email text not null unique,"
                  "  active boolean not null"
                  ")");
}

void truncate_temp_table(Connection &connection) {
  connection.exec("truncate table " + table_name());
}

void test_connection_opens(Connection &connection) {
  expect(connection.is_open(), "connection should be open");
}

void test_exec_select(Connection &connection) {
  const Result result = connection.exec("select 1 as value");

  expect(result.valid(), "exec() result should be valid");
  expect(result.size() == 1, "select 1 should return exactly one row");
  expect(!result.empty(), "select 1 should not be empty");

  const auto row = result[0];
  expect(row.size() == 1, "select 1 row should have one column");
  expect(row.contains("value"), "row should contain aliased column");
  expect(row[0].name() == "value", "column name should match alias");
  expect(row[0].view() == "1", "raw field view should equal 1");
  expect(row["value"].as_int64() == 1, "typed field conversion should return 1");
}

void test_exec_params(Connection &connection) {
  const Result result = connection.exec_params(
      "select $1::bigint as id, $2::text as email, $3::boolean as active",
      {
          DbParam{"id", std::int64_t{42}},
          DbParam{"email", std::string{"alice@example.com"}},
          DbParam{"active", true},
      });

  expect(result.size() == 1, "exec_params should return one row");

  const auto row = result[0];
  expect(row["id"].as_int64() == 42, "positional bigint param should bind");
  expect(row["email"].as_string() == "alice@example.com",
         "positional text param should bind");
  expect(row["active"].as_bool(), "positional bool param should bind");
}

void test_prepare_and_exec_prepared(Connection &connection) {
  connection.prepare("select_prepared_user",
                     "select $1::bigint as id, $2::text as email");

  const Result result = connection.exec_prepared(
      "select_prepared_user",
      {
          DbParam{"id", std::int64_t{7}},
          DbParam{"email", std::string{"prepared@example.com"}},
      });

  expect(result.size() == 1, "exec_prepared should return one row");
  const auto row = result[0];
  expect(row["id"].as_int64() == 7, "prepared bigint param should bind");
  expect(row["email"].as_string() == "prepared@example.com",
         "prepared text param should bind");
}

void test_invalid_sql_throws_query_error(Connection &connection) {
  expect_throws<QueryError>("invalid SQL should map to QueryError",
                            [&]() { connection.exec("select from"); });
}

void test_constraint_violation(Connection &connection) {
  truncate_temp_table(connection);

  connection.exec_params(
      insert_user_sql(),
      {
          DbParam{"id", std::int64_t{1}},
          DbParam{"email", std::string{"duplicate@example.com"}},
          DbParam{"active", true},
      });

  expect_throws<ConstraintViolation>(
      "duplicate unique value should map to ConstraintViolation", [&]() {
        connection.exec_params(
            insert_user_sql(),
            {
                DbParam{"id", std::int64_t{2}},
                DbParam{"email", std::string{"duplicate@example.com"}},
                DbParam{"active", false},
            });
      });
}

void test_named_field_access(Connection &connection) {
  const Result result = connection.exec(
      "select 100::bigint as id, 'named@example.com'::text as email");

  const auto row = result[0];
  expect(row.contains("email"), "row should report named field presence");
  expect(row["email"].as_string() == "named@example.com",
         "named field access should return string value");
}

void test_mapping_error(Connection &connection) {
  const Result result = connection.exec("select 'not-a-number'::text as payload");

  expect_throws<MappingError>("bad conversion should map to MappingError", [&]() {
    static_cast<void>(result[0]["payload"].as_int64());
  });
}

void test_out_of_range(Connection &connection) {
  const Result result = connection.exec("select 1 as value");

  expect_throws<std::out_of_range>("row index out of range should throw",
                                   [&]() { static_cast<void>(result[1]); });

  const auto row = result[0];
  expect_throws<std::out_of_range>("column index out of range should throw",
                                   [&]() { static_cast<void>(row[1]); });
}

struct TestCase {
  std::string_view name;
  std::function<void()> run;
};

} // namespace

int main() {
  try {
    Connection connection{make_test_config()};
    create_temp_schema(connection);

    const std::vector<TestCase> tests{
        {"connection opens", [&]() { test_connection_opens(connection); }},
        {"exec select", [&]() { test_exec_select(connection); }},
        {"exec params", [&]() { test_exec_params(connection); }},
        {"prepare + exec prepared",
         [&]() { test_prepare_and_exec_prepared(connection); }},
        {"invalid sql -> query error",
         [&]() { test_invalid_sql_throws_query_error(connection); }},
        {"constraint violation",
         [&]() { test_constraint_violation(connection); }},
        {"named field access", [&]() { test_named_field_access(connection); }},
        {"mapping error", [&]() { test_mapping_error(connection); }},
        {"out of range", [&]() { test_out_of_range(connection); }},
    };

    std::size_t passed = 0;
    for (const auto &test : tests) {
      test.run();
      ++passed;
      std::cout << "[PASS] " << test.name << '\n';
    }

    std::cout << "Passed " << passed << " integration tests." << '\n';
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
