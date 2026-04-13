#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Config.hpp"
#include "WsApp.hpp"
#include "WsProtocol.hpp"
#include "WsRouter.hpp"
#include "WsServer.hpp"

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
using ws::server::WsAcceptingDecision;
using ws::server::json;

constexpr uint16_t kTestPort = 19091;
const std::filesystem::path kConfigPath =
    std::filesystem::temp_directory_path() / "ws_app_logic_test_config.json";

void expect(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
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
        {"enable_deflate", false}}}};

  std::ofstream stream(kConfigPath);
  expect(stream.is_open(), "test config file should be writable");
  stream << config.dump(2);
}

bool can_connect_tcp(uint16_t port) {
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

void wait_for_server_ready(uint16_t port) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    if (can_connect_tcp(port)) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  expect(false, "server should start listening");
}

http::response<http::string_body>
send_upgrade_request(std::string_view target,
                     const std::vector<std::pair<std::string, std::string>>
                         &extra_headers = {}) {
  net::io_context ioc;
  tcp::resolver resolver(ioc);
  beast::tcp_stream stream(ioc);
  auto endpoints = resolver.resolve("127.0.0.1", std::to_string(kTestPort));
  stream.connect(endpoints);

  http::request<http::string_body> req{http::verb::get, std::string(target), 11};
  req.set(http::field::host, "127.0.0.1:" + std::to_string(kTestPort));
  req.set(http::field::user_agent, "WsAppLogicTest");
  req.set(http::field::upgrade, "websocket");
  req.set(http::field::connection, "Upgrade");
  req.set(http::field::sec_websocket_version, "13");
  req.set(http::field::sec_websocket_key, "dGhlIHNhbXBsZSBub25jZQ==");
  for (const auto &[key, value] : extra_headers) {
    req.set(key, value);
  }

  http::write(stream, req);
  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  http::read(stream, buffer, res);
  beast::error_code ec;
  stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  return res;
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

  void write_text(std::string_view payload) {
    m_ws.write(net::buffer(payload.data(), payload.size()));
  }

  std::string read_text() {
    beast::flat_buffer buffer;
    m_ws.read(buffer);
    return beast::buffers_to_string(buffer.data());
  }

private:
  net::io_context m_ioc;
  tcp::resolver m_resolver;
  websocket::stream<beast::tcp_stream> m_ws;
};

json read_message_payload(const std::string &text) {
  auto message = ws::server::string_to_message(text);
  expect(message.has_value(), "response should be valid JsonMessage text");
  return message->payload;
}

void test_app_rejects_unknown_endpoint_with_404() {
  ws::server::WsApp app(kConfigPath.string());
  app.add_message_handler("/chat", "ping",
                          [](ws::server::WsSession &, const JsonMessage &) {});
  app.run();
  wait_for_server_ready(kTestPort);

  const auto response = send_upgrade_request("/missing");

  app.stop();
  expect(response.result() == http::status::not_found,
         "unknown endpoint should return HTTP 404");
  expect(response.body() == "Unknown endpoint",
         "unknown endpoint rejection body should explain the reason");
}

void test_server_rejects_unauthorized_upgrade_with_401() {
  auto &config = core::config::Config::instance(kConfigPath);
  ws::server::WsServer server(config);
  server.on_auth([](ws::server::WsSession &session) {
    if (!session.header("authorization").has_value()) {
      return WsAcceptingDecision::reject_unauthorized("Missing Authorization");
    }
    // session.set_authenticated_user("authorized-user");
    return WsAcceptingDecision::default_accept();
  });
  server.start();
  wait_for_server_ready(kTestPort);

  const auto rejected = send_upgrade_request("/secure");
  const auto accepted =
      send_upgrade_request("/secure", {{"Authorization", "Bearer token"}});

  server.stop();
  expect(rejected.result() == http::status::unauthorized,
         "missing credentials should return HTTP 401");
  expect(rejected.body() == "Missing Authorization",
         "401 body should include auth failure reason");
  expect(accepted.result() == http::status::switching_protocols,
         "authorized upgrade should complete the websocket handshake");
}

void test_app_dispatches_typed_handler_by_endpoint_and_type() {
  ws::server::WsApp app(kConfigPath.string());
  app.add_message_handler("/chat", "ping",
                          [](ws::server::WsSession &session,
                             const JsonMessage &message) {
                            JsonMessage reply{};
                            reply.type = "pong";
                            reply.payload = {{"seen_type", message.type},
                                             {"value", message.payload["value"]}};
                            session.send_message(reply);
                          });
  app.run();
  wait_for_server_ready(kTestPort);

  TestWsClient client("/chat");
  JsonMessage request{};
  request.type = "ping";
  request.payload = {{"value", 7}};
  client.write_text(ws::server::message_to_json(request).dump());

  const auto payload = read_message_payload(client.read_text());

  app.stop();
  expect(payload["seen_type"] == "ping",
         "typed route should dispatch the matching message handler");
  expect(payload["value"] == 7,
         "typed route should receive the original payload");
}

// void test_app_endpoint_handler_receives_auth_and_request_context() {
//   ws::server::WsApp app(kConfigPath.string());
//   app.add_endpoint("/inspect",
//                    [](ws::server::WsSession &session, const JsonMessage &) {
//                      JsonMessage reply{};
//                      reply.type = "inspect_reply";
//                      reply.payload = {
//                          {"user", session.authenticated_user().value_or("")},
//                          {"endpoint", session.endpoint()},
//                          {"query", session.query_string()},
//                          {"header",
//                           session.header("x-test-header").value_or("")},
//                      };
//                      session.send_message(reply);
//                    });
//   app.run();
//   wait_for_server_ready(kTestPort);

//   TestWsClient client("/inspect?foo=bar",
//                       {{"X-Test-Header", "demo-header-value"}});
//   JsonMessage request{};
//   request.payload = {{"action", "inspect"}};
//   client.write_text(ws::server::message_to_json(request).dump());

//   const auto payload = read_message_payload(client.read_text());

//   app.stop();
//   expect(payload["endpoint"] == "/inspect",
//          "endpoint handler should see the parsed request endpoint");
//   expect(payload["query"] == "foo=bar",
//          "endpoint handler should see the parsed query string");
//   expect(payload["header"] == "demo-header-value",
//          "endpoint handler should see custom request headers");
//   expect(payload["user"].is_string() &&
//              payload["user"].get<std::string>().rfind("user_", 0) == 0,
//          "auth hook should attach an authenticated user to the session");
// }

void test_app_unknown_type_does_not_break_the_session() {
  ws::server::WsApp app(kConfigPath.string());
  app.add_message_handler("/chat", "ping",
                          [](ws::server::WsSession &session,
                             const JsonMessage &message) {
                            JsonMessage reply{};
                            reply.type = "pong";
                            reply.payload = {{"value", message.payload["value"]}};
                            session.send_message(reply);
                          });
  app.run();
  wait_for_server_ready(kTestPort);

  TestWsClient client("/chat");
  JsonMessage unknown{};
  unknown.type = "missing";
  client.write_text(ws::server::message_to_json(unknown).dump());
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  JsonMessage valid{};
  valid.type = "ping";
  valid.payload = {{"value", 11}};
  client.write_text(ws::server::message_to_json(valid).dump());

  const auto payload = read_message_payload(client.read_text());

  app.stop();
  expect(payload["value"] == 11,
         "unknown typed message should not close the session");
}

void test_server_protocol_error_handler_allows_follow_up_valid_message() {
  auto &config = core::config::Config::instance(kConfigPath);
  ws::server::WsServer server(config);
  server.on_protocol_error([](ws::server::WsSession &session,
                              const std::string &) { session.send_text("PROTOERR"); });
  server.on_typed_message([](ws::server::WsSession &session,
                             const JsonMessage &message) {
    JsonMessage reply{};
    reply.type = "echo";
    reply.payload = {{"value", message.payload["value"]}};
    session.send_message(reply);
  });
  server.start();
  wait_for_server_ready(kTestPort);

  TestWsClient client("/");
  client.write_text("{\"broken\"");
  expect(client.read_text() == "PROTOERR",
         "protocol error handler should run on malformed JSON");

  JsonMessage valid{};
  valid.type = "echo";
  valid.payload = {{"value", 42}};
  client.write_text(ws::server::message_to_json(valid).dump());
  const auto payload = read_message_payload(client.read_text());

  server.stop();
  expect(payload["value"] == 42,
         "session should remain usable after protocol error handling");
}

void test_server_stop_refuses_new_connections() {
  auto &config = core::config::Config::instance(kConfigPath);
  ws::server::WsServer server(config);
  server.start();
  wait_for_server_ready(kTestPort);
  server.stop();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  expect(!can_connect_tcp(kTestPort),
         "server.stop() should stop accepting new TCP connections");
}

} // namespace

int main() {
  write_test_config();

  test_app_rejects_unknown_endpoint_with_404();
  test_server_rejects_unauthorized_upgrade_with_401();
  test_app_dispatches_typed_handler_by_endpoint_and_type();
  // test_app_endpoint_handler_receives_auth_and_request_context();
  test_app_unknown_type_does_not_break_the_session();
  test_server_protocol_error_handler_allows_follow_up_valid_message();
  test_server_stop_refuses_new_connections();

  std::cout << "WsAppLogicTest passed\n";
  return EXIT_SUCCESS;
}
