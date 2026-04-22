#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "App.hpp"
#include "AuthStructs.hpp"
#include "Config.hpp"
#include "ListDevicesRequestDto.hpp"
#include "SetDeviceParameterRequestDto.hpp"
#include "StartConnectRequestDto.hpp"
#include "StartDisconnectRequestDto.hpp"
#include "StartStreamRequestDto.hpp"
#include "StopStreamRequestDto.hpp"
#include "SubscribeTelemetryRequestDto.hpp"
#include "UnsubscribeTelemetryRequestDto.hpp"
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
using tcp = net::ip::tcp;
using ws::server::JsonMessage;
using ws::server::json;

constexpr std::uint16_t kTestPort = 19092;
constexpr std::string_view kJwtSecretEnv = "COURSE_WORK_APP_TEST_JWT_SECRET";
constexpr std::string_view kJwtSecret = "course-work-secret";
constexpr std::string_view kDeviceId = "emu-camera-01";
constexpr std::string_view kStreamTopic = "telemetry.temperature";
constexpr std::string_view kParameterTopic = "parameter.gain";

const std::filesystem::path kConfigPath =
    std::filesystem::temp_directory_path() / "app_emulation_test_config.json";

void expect(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void set_env(std::string_view key, std::string_view value) {
#ifdef _WIN32
  expect(_putenv_s(std::string(key).c_str(), std::string(value).c_str()) == 0,
         "test environment variable should be set");
#else
  expect(setenv(std::string(key).c_str(), std::string(value).c_str(), 1) == 0,
         "test environment variable should be set");
#endif
}

void write_test_config() {
  const json config = {
      {"server",
       {{"host", "127.0.0.1"},
        {"port", 228},
        {"request_timeout_ms", 5000},
        {"read_timeout_ms", 5000},
        {"write_timeout_ms", 5000},
        {"max_connections", 64}}},
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
      {"auth",
       {{"jwt",
         {{"issuer", "auth-service"},
          {"audience", "server-api"},
          {"ttl_seconds", 900},
          {"secret_env", std::string(kJwtSecretEnv)}}}}}};

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

  return "Bearer " +
         infrastructure::auth::create_jwt(user, cfg, "app-emulation-test");
}

template <typename Dto>
JsonMessage make_request(const Dto &dto, std::string request_id) {
  ws::dto::WsRequestContextDto context{};
  context.request_id = std::move(request_id);
  context.method = std::string(Dto::method_name);
  context.version = "1";
  context.user_id = "test-client";
  return ws::dto::dto_to_json_message(dto, context);
}

class TestWsClient {
public:
  explicit TestWsClient(
      std::string_view target,
      const std::vector<std::pair<std::string, std::string>> &extra_headers = {})
      : m_resolver(m_ioc), m_ws(m_ioc) {
    auto endpoints = m_resolver.resolve("127.0.0.1", std::to_string(kTestPort));
    beast::get_lowest_layer(m_ws).connect(endpoints);
    m_ws.set_option(websocket::stream_base::decorator(
        [extra_headers](websocket::request_type &req) {
          for (const auto &[key, value] : extra_headers) {
            req.set(key, value);
          }
        }));
    m_ws.handshake("127.0.0.1:" + std::to_string(kTestPort), std::string(target));
  }

  ~TestWsClient() {
    beast::error_code ec;
    m_ws.close(websocket::close_code::normal, ec);
  }

  void write_message(const JsonMessage &message) {
    const auto text = ws::server::message_to_json(message).dump();
    m_ws.write(net::buffer(text.data(), text.size()));
  }

  JsonMessage await_response(std::string_view request_id) {
    return await_message([request_id](const JsonMessage &message) {
      return message.kind == "response" && message.id == request_id;
    });
  }

  JsonMessage await_event(std::string_view type, std::string_view device_id = {}) {
    return await_message([type, device_id](const JsonMessage &message) {
      if (message.kind != "event" || message.type != type ||
          !message.payload.is_object()) {
        return false;
      }
      if (device_id.empty()) {
        return true;
      }
      const auto it = message.payload.find("device_id");
      return it != message.payload.end() && it->is_string() &&
             it->get<std::string>() == device_id;
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

void expect_ok_response(const JsonMessage &response) {
  expect(response.payload.is_object(), "response payload should be an object");
  const auto status_it = response.payload.find("status");
  expect(status_it != response.payload.end() && status_it->is_object(),
         "response should contain status");
  const auto ok_it = status_it->find("ok");
  expect(ok_it != status_it->end() && ok_it->is_boolean() && ok_it->get<bool>(),
         "response status should be ok");
}

void test_app_bootstraps_emulated_devices_and_exposes_capabilities() {
  application::app::App app(kConfigPath.string());
  app.run();
  wait_for_server_ready(kTestPort);

  {
    TestWsClient client("/", {{"Authorization", make_auth_header()}});

    ws::dto::ListDevicesRequestDto list_request{};
    list_request.include_disconnected = true;
    list_request.include_capabilities = true;
    const auto request = make_request(list_request, "req-list");
    client.write_message(request);

    const auto response = client.await_response(request.id);
    expect_ok_response(response);

    const auto devices_it = response.payload.find("devices");
    expect(devices_it != response.payload.end() && devices_it->is_array(),
           "device.list should return devices array");
    expect(devices_it->size() == 3,
           "app should bootstrap three emulated devices");

    bool found_camera = false;
    for (const auto &device : *devices_it) {
      expect(device.is_object(), "each device entry should be an object");
      const auto id_it = device.find("id");
      expect(id_it != device.end() && id_it->is_string(),
             "device entry should contain id");
      if (id_it->get<std::string>() != kDeviceId) {
        continue;
      }

      found_camera = true;
      const auto capabilities_it = device.find("capabilities");
      expect(capabilities_it != device.end() && capabilities_it->is_array() &&
                 !capabilities_it->empty(),
             "device.list should include capabilities when requested");
    }

    expect(found_camera, "bootstrapped emulated camera should be present");
  }

  app.stop();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void test_app_handles_device_lifecycle_and_telemetry_events() {
  application::app::App app(kConfigPath.string());
  app.run();
  wait_for_server_ready(kTestPort);

  {
    TestWsClient client("/", {{"Authorization", make_auth_header()}});

    auto connect_request = make_request(
        ws::dto::StartConnectRequestDto{.device_id = std::string(kDeviceId)},
        "req-connect");
    client.write_message(connect_request);
    expect_ok_response(client.await_response(connect_request.id));

    auto get_state_request = make_request(
        ws::dto::GetDeviceStateRequestDto{.device_id = std::string(kDeviceId),
                                          .include_capabilities = true},
        "req-state");
    client.write_message(get_state_request);
    const auto state_response = client.await_response(get_state_request.id);
    expect_ok_response(state_response);

    const auto snapshot_it = state_response.payload.find("snapshot");
    expect(snapshot_it != state_response.payload.end() &&
               snapshot_it->is_object(),
           "device.get_state should return snapshot");
    expect((*snapshot_it)["connected"] == true,
           "connected device should report connected=true");
    expect((*snapshot_it)["state"] == "connected",
           "connected device should report connected state");
    const auto snapshot_capabilities_it = snapshot_it->find("capabilities");
    expect(snapshot_capabilities_it != snapshot_it->end() &&
               snapshot_capabilities_it->is_array() &&
               !snapshot_capabilities_it->empty(),
           "device.get_state should include capabilities when requested");

    ws::dto::SubscribeTelemetryRequestDto subscribe_request{};
    subscribe_request.device_id = std::string(kDeviceId);
    subscribe_request.topics = {std::string("device.state"),
                                std::string(kParameterTopic),
                                std::string(kStreamTopic)};
    subscribe_request.include_snapshot = true;
    const auto subscribe_message =
        make_request(subscribe_request, "req-subscribe");
    client.write_message(subscribe_message);
    expect_ok_response(client.await_response(subscribe_message.id));

    const auto state_snapshot_event = client.await_event("device.state", kDeviceId);
    const auto state_snapshot_payload = state_snapshot_event.payload;
    expect(state_snapshot_payload.contains("snapshot"),
           "telemetry.subscribe with snapshots should emit state snapshot");
    expect(state_snapshot_payload["snapshot"]["connected"] == true,
           "state snapshot should reflect current connection state");

    const auto set_param_request = make_request(
        ws::dto::SetDeviceParameterRequestDto{.device_id = std::string(kDeviceId),
                                              .parameter_name = "gain",
                                              .value = "42"},
        "req-set-param");
    client.write_message(set_param_request);
    expect_ok_response(client.await_response(set_param_request.id));

    const auto parameter_event = client.await_event(kParameterTopic, kDeviceId);
    expect(parameter_event.payload["topic"].is_string() &&
               parameter_event.payload["topic"].get<std::string>() ==
                   std::string(kParameterTopic),
           "set_parameter should publish parameter telemetry");
    expect(parameter_event.payload["value"] == "42",
           "parameter telemetry should carry the updated value");

    const auto start_stream_request = make_request(
        ws::dto::StartStreamRequestDto{.device_id = std::string(kDeviceId),
                                       .stream_name = std::string(kStreamTopic),
                                       .sample_rate_hz = 4,
                                       .replay_last_value = true},
        "req-start-stream");
    client.write_message(start_stream_request);
    expect_ok_response(client.await_response(start_stream_request.id));

    const auto stream_event = client.await_event(kStreamTopic, kDeviceId);
    expect(stream_event.payload["topic"].is_string() &&
               stream_event.payload["topic"].get<std::string>() ==
                   std::string(kStreamTopic),
           "start_stream should publish telemetry for subscribed stream");
    expect(stream_event.payload["value"].is_string() &&
               !stream_event.payload["value"].get<std::string>().empty(),
           "stream telemetry should include a sample value");

    const auto stop_stream_request = make_request(
        ws::dto::StopStreamRequestDto{.device_id = std::string(kDeviceId),
                                      .stream_name = std::string(kStreamTopic)},
        "req-stop-stream");
    client.write_message(stop_stream_request);
    expect_ok_response(client.await_response(stop_stream_request.id));

    const auto unsubscribe_request = make_request(
        ws::dto::UnsubscribeTelemetryRequestDto{
            .device_id = std::string(kDeviceId),
            .topics = {std::string("device.state"),
                       std::string(kParameterTopic), std::string(kStreamTopic)}},
        "req-unsubscribe");
    client.write_message(unsubscribe_request);
    expect_ok_response(client.await_response(unsubscribe_request.id));

    const auto disconnect_request = make_request(
        ws::dto::StartDisconnectRequestDto{.device_id = std::string(kDeviceId),
                                           .force = false,
                                           .reason = "test complete"},
        "req-disconnect");
    client.write_message(disconnect_request);
    expect_ok_response(client.await_response(disconnect_request.id));

    const auto final_state_request = make_request(
        ws::dto::GetDeviceStateRequestDto{.device_id = std::string(kDeviceId)},
        "req-final-state");
    client.write_message(final_state_request);
    const auto final_state_response =
        client.await_response(final_state_request.id);
    expect_ok_response(final_state_response);
    expect(final_state_response.payload["snapshot"]["connected"] == false,
           "disconnect should return device to disconnected state");
    expect(final_state_response.payload["snapshot"]["state"] == "disconnected",
           "disconnect should report disconnected state");
  }

  app.stop();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

} // namespace

int main() {
  set_env(kJwtSecretEnv, kJwtSecret);
  write_test_config();

  test_app_bootstraps_emulated_devices_and_exposes_capabilities();
  test_app_handles_device_lifecycle_and_telemetry_events();

  std::cout << "AppEmulationTest passed\n";
  return EXIT_SUCCESS;
}
