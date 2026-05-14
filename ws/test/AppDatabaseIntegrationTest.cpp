#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "App.hpp"
#include "AuthStructs.hpp"
#include "Connection.hpp"
#include "ConnectionConfig.hpp"
#include "DbDeviceCatalog.hpp"
#include "ListDevicesRequestDto.hpp"
#include "SetDeviceParameterRequestDto.hpp"
#include "SubscribeTelemetryRequestDto.hpp"
#include "WsProtocol.hpp"
#include "WsRequestContextDto.hpp"
#include "WsRequestDtoMapper.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

namespace {

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using db::core::Connection;
using db::core::ConnectionConfig;
using infrastructure::device::DbDeviceCatalog;
using infrastructure::device::DbDeviceCatalogConfig;
using ws::server::json;
using ws::server::JsonMessage;
using tcp = net::ip::tcp;

constexpr std::uint16_t kTestPort = 19093;
constexpr std::string_view kJwtSecretEnv = "COURSE_WORK_APP_TEST_JWT_SECRET";
constexpr std::string_view kJwtSecret = "course-work-secret";
constexpr std::string_view kDeviceId = "emu-camera-01";
constexpr std::string_view kParameterTopic = "parameter.gain";

const std::filesystem::path kConfigPath = std::filesystem::temp_directory_path() / "app_database_integration_test_config.json";

void expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void set_env(std::string_view key, std::string_view value) {
#ifdef _WIN32
  expect(_putenv_s(std::string(key).c_str(), std::string(value).c_str()) == 0, "test environment variable should be set");
#else
  expect(setenv(std::string(key).c_str(), std::string(value).c_str(), 1) == 0, "test environment variable should be set");
#endif
}

std::string env_or(const char* name, std::string default_value) {
  if (const char* value = std::getenv(name); value != nullptr && *value != '\0') {
    return value;
  }
  return default_value;
}

std::uint16_t env_port_or(const char* name, std::uint16_t default_value) {
  if (const char* value = std::getenv(name); value != nullptr && *value != '\0') {
    const auto parsed = std::stoi(value);
    if (parsed <= 0 || parsed > 65535) {
      throw std::runtime_error(std::string(name) + " must be in range 1..65535");
    }
    return static_cast<std::uint16_t>(parsed);
  }
  return default_value;
}

std::filesystem::path find_repo_root() {
  auto current = std::filesystem::absolute(std::filesystem::path(__FILE__));
  if (current.has_filename()) {
    current = current.parent_path();
  }

  while (!current.empty()) {
    if (std::filesystem::exists(current / "sql" / "tables.sql") && std::filesystem::exists(current / "sql" / "indexes.sql")) {
      return current;
    }

    const auto parent = current.parent_path();
    if (parent == current) {
      break;
    }
    current = parent;
  }

  throw std::runtime_error("Repository root with sql/ schema files was not found");
}

ConnectionConfig make_db_connection_config() {
  ConnectionConfig config;
  config.host = env_or("DB_TEST_HOST", "127.0.0.1");
  config.port = env_port_or("DB_TEST_PORT", 5432);
  config.dbname = env_or("DB_TEST_DBNAME", "postgres");
  config.user = env_or("DB_TEST_USER", "postgres");
  config.password = env_or("DB_TEST_PASSWORD", "postgres");
  config.application_name = "app_database_integration_test";
  return config;
}

DbDeviceCatalogConfig make_catalog_config() {
  const auto repo_root = find_repo_root();
  const auto db_connection = make_db_connection_config();

  DbDeviceCatalogConfig config;
  config.host = db_connection.host;
  config.port = db_connection.port;
  config.dbname = db_connection.dbname;
  config.user = db_connection.user;
  config.password = db_connection.password;
  config.application_name = "app_database_integration_test";
  config.max_connections = 4;
  config.min_connections = 1;
  config.auto_apply_schema = true;
  config.tables_sql_path = repo_root / "sql" / "tables.sql";
  config.indexes_sql_path = repo_root / "sql" / "indexes.sql";
  return config;
}

void write_test_config() {
  const auto repo_root = find_repo_root();
  const auto db_connection = make_db_connection_config();

  const json config = {{"database",
                        {{"app",
                          {{"enabled", true},
                           {"host", db_connection.host},
                           {"port", db_connection.port},
                           {"dbname", db_connection.dbname},
                           {"user", db_connection.user},
                           {"password", db_connection.password},
                           {"application_name", "app_database_integration_test"},
                           {"auto_apply_schema", true},
                           {"pool", {{"max_connections", 4}, {"min_connections", 1}}},
                           {"schema", {{"tables_path", (repo_root / "sql" / "tables.sql").string()}, {"indexes_path", (repo_root / "sql" / "indexes.sql").string()}}}}}}},
                       {"server", {{"host", "127.0.0.1"}, {"port", 228}, {"request_timeout_ms", 5000}, {"read_timeout_ms", 5000}, {"write_timeout_ms", 5000}, {"max_connections", 64}}},
                       {"websocket",
                        {{"port", kTestPort},
                         {"drain_timeout", 1},
                         {"idle_timeout", 60},
                         {"ping_interval", 30},
                         {"max_message_size", 65536},
                         {"max_queued_bytes", 65536},
                         {"max_queued_messages", 256},
                         {"enable_deflate", false}}},
                       {"emulation", {{"device_count", 3}}},
                       {"auth", {{"jwt", {{"issuer", "auth-service"}, {"audience", "server-api"}, {"ttl_seconds", 900}, {"secret_env", std::string(kJwtSecretEnv)}}}}}};

  std::ofstream stream(kConfigPath);
  expect(stream.is_open(), "test config file should be writable");
  stream << config.dump(2);
}

bool can_connect_tcp(std::uint16_t port) {
  net::io_context ioc;
  tcp::socket socket(ioc);
  beast::error_code ec;
  socket.connect({net::ip::make_address("127.0.0.1", ec), port}, ec);
  if (!ec) {
    socket.close(ec);
    return true;
  }
  return false;
}

void wait_for_server_ready(std::uint16_t port) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    if (can_connect_tcp(port)) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  expect(false, "server should start listening");
}

std::string make_auth_header() {
  infrastructure::auth::JwtConfig cfg{
      .issuer = "auth-service",
      .audience = "server-api",
      .secret = std::string(kJwtSecret),
      .ttl = std::chrono::seconds(900),
  };
  infrastructure::auth::UserClaims user{
      .user_id = "operator-1",
      .email = "operator@example.com",
      .name = "Operator One",
      .role = "operator",
      .tenant_id = "course-work",
      .permissions = {"devices:read", "devices:control", "telemetry:read"},
  };

  return "Bearer " + infrastructure::auth::create_jwt(user, cfg, "app-database-integration-test");
}

template <typename Dto>
JsonMessage make_request(const Dto& dto, std::string request_id) {
  ws::dto::WsRequestContextDto context{};
  context.request_id = std::move(request_id);
  context.method = std::string(Dto::method_name);
  context.version = "1";
  context.user_id = "test-client";
  return ws::dto::dto_to_json_message(dto, context);
}

class TestWsClient {
  public:
  explicit TestWsClient(std::string_view target, const std::vector<std::pair<std::string, std::string>>& extra_headers = {})
      : m_resolver(m_ioc)
      , m_ws(m_ioc) {
    auto endpoints = m_resolver.resolve("127.0.0.1", std::to_string(kTestPort));
    beast::get_lowest_layer(m_ws).connect(endpoints);
    m_ws.set_option(websocket::stream_base::decorator([extra_headers](websocket::request_type& req) {
      for (const auto& [key, value] : extra_headers) {
        req.set(key, value);
      }
    }));
    m_ws.handshake("127.0.0.1:" + std::to_string(kTestPort), std::string(target));
  }

  ~TestWsClient() {
    beast::error_code ec;
    m_ws.close(websocket::close_code::normal, ec);
  }

  void write_message(const JsonMessage& message) {
    const auto text = ws::server::message_to_json(message).dump();
    m_ws.write(net::buffer(text.data(), text.size()));
  }

  JsonMessage await_response(std::string_view request_id) {
    return await_message([request_id](const JsonMessage& message) { return message.kind == "response" && message.id == request_id; });
  }

  JsonMessage await_event(std::string_view type, std::string_view device_id = {}) {
    return await_message([type, device_id](const JsonMessage& message) {
      if (message.kind != "event" || message.type != type || !message.payload.is_object()) {
        return false;
      }
      if (device_id.empty()) {
        return true;
      }
      const auto it = message.payload.find("device_id");
      return it != message.payload.end() && it->is_string() && it->get<std::string>() == device_id;
    });
  }
  private:
  template <typename Predicate>
  JsonMessage await_message(Predicate predicate) {
    while (true) {
      for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        if (predicate(*it)) {
          auto message = std::move(*it);
          m_pending.erase(it);
          return message;
        }
      }

      auto message = read_message();
      if (predicate(message)) {
        return message;
      }
      m_pending.push_back(std::move(message));
    }
  }

  JsonMessage read_message() {
    beast::get_lowest_layer(m_ws).expires_after(std::chrono::seconds(3));
    beast::flat_buffer buffer;
    beast::error_code ec;
    m_ws.read(buffer, ec);
    expect(!ec, "websocket read should succeed");

    const auto text = beast::buffers_to_string(buffer.data());
    auto parsed = ws::server::string_to_message(text);
    expect(parsed.has_value(), "response should be valid JsonMessage text");
    return *parsed;
  }
  private:
  net::io_context m_ioc;
  tcp::resolver m_resolver;
  websocket::stream<beast::tcp_stream> m_ws;
  std::deque<JsonMessage> m_pending{};
};

void expect_ok_response(const JsonMessage& response) {
  expect(response.payload.is_object(), "response payload should be an object");
  const auto status_it = response.payload.find("status");
  expect(status_it != response.payload.end() && status_it->is_object(), "response should contain status");
  const auto ok_it = status_it->find("ok");
  expect(ok_it != status_it->end() && ok_it->is_boolean() && ok_it->get<bool>(), "response status should be ok");
}

void reset_catalog() {
  DbDeviceCatalog catalog(make_catalog_config());
  Connection connection(make_db_connection_config());
  static_cast<void>(connection.exec("truncate table device_tags, devices"));
}

std::size_t catalog_device_count() {
  DbDeviceCatalog catalog(make_catalog_config());
  return catalog.count_devices();
}

std::optional<std::string> catalog_parameter_value(std::string_view device_id, std::string_view name) {
  DbDeviceCatalog catalog(make_catalog_config());
  auto record = catalog.find_device(device_id);
  if (!record.has_value()) {
    return std::nullopt;
  }

  const auto it = record->parameters.find(std::string(name));
  if (it == record->parameters.end()) {
    return std::nullopt;
  }
  return it->second;
}

void wait_for_persisted_parameter(std::string_view device_id, std::string_view name, std::string_view expected_value) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    const auto persisted = catalog_parameter_value(device_id, name);
    if (persisted.has_value() && *persisted == expected_value) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  expect(false, "parameter should be persisted to the device catalog");
}

void test_app_seeds_catalog_and_reloads_persisted_parameters() {
  reset_catalog();
  expect(catalog_device_count() == 0, "catalog should be empty before startup");

  {
    application::app::App app(kConfigPath.string());
    app.run();
    wait_for_server_ready(kTestPort);

    TestWsClient client("/", {{"Authorization", make_auth_header()}});

    ws::dto::ListDevicesRequestDto list_request{};
    list_request.include_disconnected = true;
    list_request.include_capabilities = true;
    client.write_message(make_request(list_request, "seed-list"));
    const auto list_response = client.await_response("seed-list");
    expect_ok_response(list_response);

    const auto devices_it = list_response.payload.find("devices");
    expect(devices_it != list_response.payload.end() && devices_it->is_array(), "device.list should return devices array");
    expect(devices_it->size() == 3, "db-backed app should seed three emulated devices on first startup");

    client.write_message(make_request(ws::dto::SetDeviceParameterRequestDto{.device_id = std::string(kDeviceId), .parameter_name = "gain", .value = "42"}, "seed-set-param"));
    expect_ok_response(client.await_response("seed-set-param"));

    wait_for_persisted_parameter(kDeviceId, "gain", "42");

    app.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  expect(catalog_device_count() == 3, "first startup should persist the default emulated fleet");
  const auto persisted_gain = catalog_parameter_value(kDeviceId, "gain");
  expect(persisted_gain.has_value() && *persisted_gain == "42", "parameter update should be stored in the catalog");

  {
    application::app::App app(kConfigPath.string());
    app.run();
    wait_for_server_ready(kTestPort);

    TestWsClient client("/", {{"Authorization", make_auth_header()}});

    ws::dto::ListDevicesRequestDto list_request{};
    list_request.include_disconnected = true;
    list_request.include_capabilities = true;
    client.write_message(make_request(list_request, "reload-list"));
    const auto list_response = client.await_response("reload-list");
    expect_ok_response(list_response);

    const auto devices_it = list_response.payload.find("devices");
    expect(devices_it != list_response.payload.end() && devices_it->is_array(), "device.list should return devices array after reload");
    expect(devices_it->size() == 3, "reloading from the catalog should not duplicate seeded devices");

    ws::dto::SubscribeTelemetryRequestDto subscribe_request{};
    subscribe_request.device_id = std::string(kDeviceId);
    subscribe_request.topics = {std::string(kParameterTopic)};
    subscribe_request.include_snapshot = true;
    client.write_message(make_request(subscribe_request, "reload-subscribe"));
    expect_ok_response(client.await_response("reload-subscribe"));

    const auto parameter_snapshot_event = client.await_event(kParameterTopic, kDeviceId);
    expect(parameter_snapshot_event.payload["snapshot"] == true, "reloaded device should emit a snapshot event for persisted parameter");
    expect(parameter_snapshot_event.payload["value"] == "42", "reloaded device should restore persisted parameter values");

    app.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  expect(catalog_device_count() == 3, "restarting with persisted devices should keep the catalog cardinality stable");
  reset_catalog();
}

} // namespace

int main() {
  set_env(kJwtSecretEnv, kJwtSecret);
  write_test_config();
  test_app_seeds_catalog_and_reloads_persisted_parameters();

  std::cout << "AppDatabaseIntegrationTest passed\n";
  return EXIT_SUCCESS;
}
