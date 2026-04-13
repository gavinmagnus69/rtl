#pragma once

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <deque>
#include <format>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/system/error_code.hpp>

#include "IAuthenticator.hpp"
#include "IExecutor.hpp"
#include "WsConfig.hpp"
#include "WsProtocol.hpp"
#include "WsRouter.hpp"

#include "boost/asio/post.hpp"
#include "boost/beast/core/flat_buffer.hpp"
#include "boost/beast/core/stream_traits.hpp"
#include "boost/beast/http/field.hpp"
#include "boost/beast/http/message_fwd.hpp"
#include "boost/beast/http/read.hpp"
#include "boost/beast/http/status.hpp"
#include "boost/beast/http/string_body_fwd.hpp"
#include "boost/beast/websocket/rfc6455.hpp"
#include "boost/beast/websocket/stream.hpp"
#include <boost/algorithm/string.hpp>

namespace ws::server {

namespace beast = boost::beast;
namespace ws = boost::beast::websocket;
namespace net = boost::asio;
namespace http = boost::beast::http;
using tcp = net::ip::tcp;
using erc = boost::system::error_code;

static std::uint64_t next_connection_id() {
  static std::atomic_uint64_t counter{1};
  return counter.fetch_add(1, std::memory_order_relaxed);
}

struct AuthInfo {
  std::string user_id;
  std::string auth_schema;
  std::vector<std::string> roles;
};

struct ConnectionInfo {
  std::string m_remoteAddress{};
  std::string m_localAddress{};
  std::string m_connectTimestamp{};
  std::chrono::system_clock::time_point connected_at;
  std::chrono::steady_clock::time_point last_activity;
  uint64_t connection_id{};
  uint16_t m_remotePort{};
  uint16_t m_localPort{};
};

struct HttpEndpoint {
  void setHttpRequest(const http::request<http::string_body> &request) {
    m_rawTarget = request.target();
    copy_headers(request);
    parse_raw_endpoint();
  }
  [[nodiscard]] const std::string &target() const noexcept {
    return m_rawTarget;
  };
  [[nodiscard]] const std::string &endpoint() const noexcept {
    return m_endpoint;
  };
  [[nodiscard]] const std::string &query_string() const noexcept {
    return m_payload;
  }

  [[nodiscard]] const std::unordered_map<std::string, std::string> &
  get_headers() const {
    return m_headers;
  };

  [[nodiscard]] std::optional<std::string>
  header(std::string_view field) const {
    auto lower_field = boost::algorithm::to_lower_copy(std::string(field));
    auto it = m_headers.find(std::string(lower_field));
    if (it != m_headers.end()) {
      return it->second;
    }
    return std::nullopt;
  };

private:
  void copy_headers(const http::request<http::string_body> &request) {
    for (const auto &field : request) {
      auto lower_field =
          boost::algorithm::to_lower_copy(std::string(field.name_string()));
      m_headers.emplace(lower_field, field.value());
    }
  };
  void parse_raw_endpoint() {
    terminatorPos = m_rawTarget.find('?');
    if (terminatorPos != std::string::npos) {
      m_endpoint = m_rawTarget.substr(0, terminatorPos);
      m_payload = m_rawTarget.substr(terminatorPos + 1,
                                     m_rawTarget.size() - terminatorPos - 1);
      return;
    }
    m_endpoint = m_rawTarget;
  }
  std::unordered_map<std::string, std::string> m_headers;
  std::string m_endpoint{};
  std::string m_rawTarget{};
  std::string m_payload{};

  size_t terminatorPos{std::string::npos};
};

struct RequestContext {
  void setHttpRequest(const http::request<http::string_body> &request) {
    httpInfo.setHttpRequest(request);
  };

  void
  setTcpSocket(const net::basic_socket<boost::asio::ip::tcp,
                                       boost::asio::any_io_executor> &socket) {
    erc error;
    auto remote_ = socket.remote_endpoint(error);
    if (!error) {
      connectionInfo.m_remoteAddress = remote_.address().to_string();
      connectionInfo.m_remotePort = remote_.port();
      connectionInfo.connected_at = std::chrono::system_clock::now();
      connectionInfo.connection_id = next_connection_id();
    }
    auto local_ = socket.local_endpoint(error);
    if (!error) {
      connectionInfo.m_localAddress = local_.address().to_string();
      connectionInfo.m_localPort = local_.port();
    }
  };

  void update_activity() {
    connectionInfo.last_activity = std::chrono::steady_clock::now();
  };

  [[nodiscard]] std::optional<std::string>
  header(std::string_view field) const {
    return httpInfo.header(field);
  };

  [[nodiscard]] const std::string &remote_address() const noexcept {
    return connectionInfo.m_remoteAddress;
  };

  [[nodiscard]] const uint16_t &remote_port() const noexcept {
    return connectionInfo.m_remotePort;
  };

  [[nodiscard]] std::string last_activity_timestamp() const noexcept {
    return format_time(connectionInfo.last_activity);
  };

  [[nodiscard]] std::string connect_timestamp() const noexcept {
    return format_time(connectionInfo.connected_at);
  };

  [[nodiscard]] const std::string &target() const noexcept {
    return httpInfo.target();
  };

  [[nodiscard]] const std::string &endpoint() const noexcept {
    return httpInfo.endpoint();
  };

  [[nodiscard]] const std::string &query_string() const noexcept {
    return httpInfo.query_string();
  };

  [[nodiscard]] const std::unordered_map<std::string, std::string> &
  get_headers() const {
    return httpInfo.get_headers();
  };

private:
  static std::string format_time(std::chrono::system_clock::time_point tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
  };
  static std::string format_time(std::chrono::steady_clock::time_point tp) {
    const auto system_now = std::chrono::system_clock::now();
    const auto steady_now = std::chrono::steady_clock::now();
    const auto estimated_system_tp =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            system_now + (tp - steady_now));
    return format_time(estimated_system_tp);
  };
  HttpEndpoint httpInfo;
  ConnectionInfo connectionInfo;
};

class WsSession : public std::enable_shared_from_this<WsSession> {
public:
  WsSession(tcp::socket socket, const WsConfig &config,
            std::shared_ptr<WsRouter> router,
            std::shared_ptr<rtl::stp::IExecutor> executor)
      : m_idleTimer(socket.get_executor()),
        m_heartbeatTimer(socket.get_executor()), m_ws(std::move(socket)),
        m_wsConfig(config), m_router(std::move(router)),
        m_executor(std::move(executor)) {
    setup_ws();
  }
  ~WsSession() { SPDLOG_INFO("WsSession destructor called."); }

  void set_authentication_principal(
      const middleware::auth::AuthenticatedPrincipal &principal) {
    m_authPrincipal = principal;
  };

  void run() { do_accept(); };

  void send_text(std::string_view message) {
    if (!is_open()) {
      return;
    }
    auto self = shared_from_this();
    std::string msg_str{message};
    net::post(m_ws.get_executor(),
              [self, msg_str = std::move(msg_str)]() mutable {
                self->do_enqueue_message(false, std::move(msg_str));
              });
  }

  void send_binary(const void *data, std::size_t size) {
    if (!is_open()) {
      return;
    }
    auto self = shared_from_this();
    std::string msg_str{static_cast<const char *>(data), size};
    net::post(m_ws.get_executor(),
              [self, msg_str = std::move(msg_str)]() mutable {
                self->do_enqueue_message(true, std::move(msg_str));
              });
  };

  void send_json(const json &js) { send_text(js.dump()); };

  void send_message(const JsonMessage &msg) {
    send_text(message_to_json(msg).dump());
  };

  void close(ws::close_reason reason = ws::close_code{}) {

    auto self = shared_from_this();
    net::post(m_ws.get_executor(), [reason = reason, self]() mutable {
      if (!self->begin_closing()) {
        return;
      }
      self->m_ws.async_close(reason, [self](const erc &ec) {
        if (ec == boost::asio::error::operation_aborted) {
          return;
        }
        if (ec) {
          SPDLOG_ERROR("WebSocket close failed: {}", ec.message());
        } else {
          SPDLOG_INFO("WebSocket closed");
        }
        self->mark_closed_and_notify_once();
      });
    });
  };

  void close_for_shutdown() {
    if (is_closing_or_closed()) {
      return;
    }
    auto self = shared_from_this();
    net::post(m_ws.get_executor(), [self]() mutable {
      self->cancel_idle_timer();
      self->m_curState = SessionState::shutdown;
      self->run_drain_timer(std::chrono::seconds(1));
      self->do_drain_queue();
    });
  };

  void close_forcefully() {
    auto self = shared_from_this();
    net::post(m_ws.get_executor(), [self]() {
      if (self->m_curState == SessionState::closed) {
        return;
      }
      self->cancel_drain_timeout();
      self->m_curState = SessionState::closing;
      self->m_ws.async_close(ws::close_code::going_away, [self](const erc &ec) {
        if (ec == boost::asio::error::operation_aborted) {
          return;
        }
        if (ec) {
          SPDLOG_ERROR("WebSocket close failed: {}", ec.message());
        }
        self->mark_closed_and_notify_once();
      });
    });
  };

  const std::string &endpoint() const noexcept {
    return m_requestContext.endpoint();
  };

  const std::string &query_string() const noexcept {
    return m_requestContext.query_string();
  };

  bool is_closed() const noexcept {
    return m_curState == SessionState::closed;
  };

  std::optional<std::string> header(std::string_view field) const {
    return m_requestContext.header(field);
  };

  const RequestContext &request_context() const noexcept {
    return m_requestContext;
  };

  const middleware::auth::AuthenticatedPrincipal &
  authenticated_principal() const {
    return m_authPrincipal.value();
  };

  bool is_authenticated() const { return m_authPrincipal.has_value(); };

private:
  void do_read() {
    if (!is_open()) {
      return;
    }
    m_ws.async_read(m_buffer, [this, self = shared_from_this()](
                                  const erc &ec, std::size_t bytes) {
      on_read(ec, bytes);
    });
  };

  void on_read(const boost::system::error_code &ec, std::size_t bytes) {
    cancel_idle_timer();
    if (ec) {
      if (ec == boost::beast::websocket::error::closed) {
        SPDLOG_INFO("WebSocket closed by client");
      } else {
        SPDLOG_ERROR("WebSocket read failed: {}", ec.message());
      }
      begin_closing();
      mark_closed_and_notify_once();
      return;
    }
    m_requestContext.update_activity();
    SPDLOG_INFO("WebSocket message received: {} bytes", bytes);
    auto data_string = beast::buffers_to_string(m_buffer.data());
    m_buffer.clear();
    if (m_router) {
      m_router->handle_message(*this, data_string);
    }
    if (is_open()) {
      run_idle_timer();
      do_read();
    }
  };

  void do_accept() {
    m_curState = SessionState::opening;
    http::async_read(m_ws.next_layer(), m_httpBuffer, m_request,
                     [this, self = shared_from_this()](const erc &ec, size_t) {
                       if (!validate_handshake_request(ec)) {
                         return;
                       }
                       m_ws.async_accept(m_request,
                                         [this, self = shared_from_this()](
                                             const erc &ec) { on_accept(ec); });
                     });
  };

  void on_accept(const boost::system::error_code &ec) {
    if (ec) {
      m_curState = SessionState::closed;
      SPDLOG_ERROR("WebSocket accept failed: {}", ec.message());
      if (m_router) {
        m_router->handle_error(*this, ec);
      }
      return;
    }
    m_curState = SessionState::open;
    // SPDLOG_INFO("WebSocket accepted: session");
    if (m_router) {
      m_router->handle_open(*this);
    }
    if (m_wsConfig.pingInterval.count() > 0) {
      do_heartbeat();
    }
    run_idle_timer();
    do_read();
  };

  void run_idle_timer() {
    if (m_wsConfig.idleTimeout.count() <= 0) {
      return;
    }
    m_idleTimer.expires_after(m_wsConfig.idleTimeout);
    auto self = shared_from_this();
    m_idleTimer.async_wait(
        [this, self](const erc &ec) { on_idle_timeout(ec); });
  };

  void cancel_idle_timer() { m_idleTimer.cancel(); };

  void on_idle_timeout(const boost::system::error_code &ec) {
    if (ec == boost::asio::error::operation_aborted) {
      return;
    }
    if (ec) {
      SPDLOG_ERROR("Idle timer error: {}", ec.message());
      return;
    }
    SPDLOG_INFO("Idle timeout, closing WebSocket session");
    close(ws::close_code::normal);
  };

  void on_write_complete(const boost::system::error_code &ec,
                         std::size_t bytes) {
    if (ec) {
      SPDLOG_ERROR("WebSocket write failed: {}", ec.message());
      begin_closing();
      mark_closed_and_notify_once();
      return;
    }
    m_requestContext.update_activity();
    SPDLOG_INFO("WebSocket message sent: {} bytes", bytes);
    do_write_next();
  };

  void reject_handshake(http::status status, std::string_view message) {
    auto response = std::make_shared<http::response<http::string_body>>(
        status, m_request.version());
    response->set(http::field::server, "ws-server");
    response->set(http::field::content_type, "text/plain");
    response->keep_alive(false);
    response->body() = std::string(message);
    response->prepare_payload();
    auto self = shared_from_this();
    http::async_write(
        m_ws.next_layer(), *response,
        [this, self, response](const erc &ec, size_t) {
          if (ec) {
            SPDLOG_ERROR("Handshake rejection write failed: {}", ec.message());
          }
          erc ignored;
          erc ans =
              m_ws.next_layer().shutdown(tcp::socket::shutdown_both, ignored);
          if (ans) {
            SPDLOG_ERROR("TCP shutdown error: {}", ans.message());
          }
          ans = m_ws.next_layer().close(ignored);
          if (ans) {
            SPDLOG_ERROR("TCP close error: {}", ans.message());
          }
          m_curState = SessionState::closed;
        });
  };

  void run_drain_timer(std::chrono::seconds drain_timeout_sec) {
    if (drain_timeout_sec.count() <= 0) {
      return;
    }
    m_idleTimer.expires_after(drain_timeout_sec);
    auto self = shared_from_this();
    m_idleTimer.async_wait(
        [this, self](const erc &ec) { on_drain_timeout(ec); });
  };

  void on_drain_timeout(const erc &ec) {
    if (ec == boost::asio::error::operation_aborted) {
      return;
    }
    if (ec) {
      SPDLOG_ERROR("Drain timer error: {}", ec.message());
      return;
    }
    SPDLOG_INFO("Drain timeout, closing WebSocket session");
    close_forcefully();
  };

  void cancel_drain_timeout() { m_idleTimer.cancel(); };

  void do_drain_queue() {
    if (m_writing) {
      return;
    }
    if (m_writeQueue.empty()) {
      m_writing = false;
      close_forcefully();
      return;
    }
    m_writing = true;
    auto self = shared_from_this();
    auto &next_msg = m_writeQueue.front();
    size_t writing_bytes = next_msg.data.size();
    m_ws.text(!next_msg.isBinary); // setting writing option
    m_ws.async_write(net::buffer(next_msg.data),
                     [this, self, to_write = writing_bytes](const erc &ec,
                                                            std::size_t bytes) {
                       if (!ec && !m_writeQueue.empty()) {
                         m_writeQueue.pop_front();
                         decrement_message_counters(to_write);
                       }
                       on_drain_message_complete(ec, bytes);
                     });
  };

  void on_drain_message_complete(const erc &ec, size_t bytes) {
    if (ec) {
      SPDLOG_ERROR("WebSocket write failed: {}", ec.message());
      close_forcefully();
      return;
    }
    m_writing = false;
    do_drain_queue();
  };

  void setup_ws() {
    {
      erc ec;
      auto ec2 = m_ws.next_layer().set_option(tcp::no_delay(true), ec);
      if (ec || ec2) {
        SPDLOG_WARN("Failed to set TCP no_delay option: {}", ec.message());
      }
    }
    m_requestContext.setTcpSocket(m_ws.next_layer().lowest_layer());
    m_ws.read_message_max(m_wsConfig.maxMessageSize);
    m_buffer.reserve(m_wsConfig.maxMessageSize);
    if (m_wsConfig.enablePerMessageDeflate) {
      m_ws.set_option(ws::permessage_deflate{true});
    }
    if (m_wsConfig.pingInterval.count() > 0) {
      m_ws.control_callback(
          [this](ws::frame_type kind, boost::string_view payload) {
            if (kind == ws::frame_type::pong) {
              stop_heartbeat_timer();
              do_heartbeat_next();
            };
            m_requestContext.update_activity();
          });
    }
  };

  void do_enqueue_message(bool isBinary, std::string message) {
    if (is_shutdown()) {
      do_write_next();
      return;
    };
    if (!is_open()) {
      return;
    }
    if (!check_message_size(message.size())) {
      queue_overflow();
      return;
    }
    m_writeQueue.push_back({isBinary, std::move(message)});
    if (!m_writing) {
      do_write_next();
    }
  };

  void do_write_next() {
    if (is_shutdown()) {
      m_writing = false;
      do_drain_queue();
      return;
    }
    if (!is_open()) {
      m_writeQueue.clear();
      m_writing = false;
      return;
    }
    // nothing to write
    if (m_writeQueue.empty()) {
      m_writing = false;
      return;
    }
    m_writing = true;
    auto self = shared_from_this();
    auto &next_msg = m_writeQueue.front();
    size_t writing_bytes = next_msg.data.size();
    m_ws.text(!next_msg.isBinary); // setting writing option
    m_ws.async_write(net::buffer(next_msg.data),
                     [this, self, to_write = writing_bytes](const erc &ec,
                                                            std::size_t bytes) {
                       if (!ec && !m_writeQueue.empty()) {
                         m_writeQueue.pop_front();
                         decrement_message_counters(to_write);
                       }
                       on_write_complete(ec, bytes);
                     });
  };
  //   accepting or closing(if rejected)
  bool validate_handshake_request(const erc &ec) {
    if (ec) {
      on_accept(ec);
      return false;
    }
    m_requestContext.setHttpRequest(m_request);
    // m_endpoint.setHttpRequest(m_request);
    if (m_router) {
      WsAcceptingDecision decision = m_router->handle_auth(*this);
      if (!decision.accept) {
        m_curState = SessionState::closing;
        reject_handshake(decision.status, decision.message);
        return false;
      }
      decision = m_router->handle_accept(*this);
      if (!decision.accept) {
        m_curState = SessionState::closing;
        reject_handshake(decision.status, decision.message);
        return false;
      }
      return true;
    };
    return true;
  };

  bool is_shutdown() const noexcept {
    return m_curState == SessionState::shutdown;
  };

  bool is_open() const noexcept { return m_curState == SessionState::open; }

  bool is_closing_or_closed() const noexcept {
    return m_curState == SessionState::closing ||
           m_curState == SessionState::closed ||
           m_curState == SessionState::shutdown;
  }

  bool begin_closing() {
    if (is_closing_or_closed()) {
      return false;
    }
    m_curState = SessionState::closing;
    cancel_idle_timer();
    cancel_drain_timeout();
    stop_heartbeat_timer();
    m_writeQueue.clear();
    m_currentQueuedBytes = 0;
    m_currentQueuedMessages = 0;
    m_writing = false;
    return true;
  }

  void mark_closed_and_notify_once() {
    m_curState = SessionState::closed;
    if (m_closeNotified) {
      return;
    }
    m_closeNotified = true;
    if (m_router) {
      m_router->handle_close(*this);
    }
    m_requestContext.update_activity();
  }

  void adjust_message_counters(size_t bytes) {
    m_currentQueuedBytes += bytes;
    m_currentQueuedMessages += 1;
  }

  void decrement_message_counters(size_t bytes) {
    m_currentQueuedBytes -= bytes;
    m_currentQueuedMessages -= 1;
  }

  bool check_message_size(size_t bytes) {
    if (m_currentQueuedMessages + 1 > m_wsConfig.maxQueuedMessages ||
        m_currentQueuedBytes + bytes > m_wsConfig.maxQueuedBytes) {
      return false;
    }
    adjust_message_counters(bytes);
    return true;
  }

  void queue_overflow() {
    m_ws.next_layer().lowest_layer().remote_endpoint();
    SPDLOG_ERROR("WsSession Overflow: queued byte {}/{}, queued messages {}/{}",
                 m_currentQueuedBytes, m_wsConfig.maxQueuedBytes,
                 m_currentQueuedMessages, m_wsConfig.maxQueuedMessages);
    close();
  };

  void run_heartbeat_timer(std::chrono::seconds heartbeat_interval_sec) {
    if (heartbeat_interval_sec.count() <= 0) {
      return;
    }
    m_heartbeatTimer.expires_after(heartbeat_interval_sec);
    auto self = shared_from_this();
    m_heartbeatTimer.async_wait(
        [this, self](const erc &ec) { on_heartbeat_timeout(ec); });
  };

  void stop_heartbeat_timer() { m_heartbeatTimer.cancel(); };

  void on_heartbeat_timeout(const erc &ec) {
    if (ec == boost::asio::error::operation_aborted) {
      return;
    }
    if (ec) {
      SPDLOG_ERROR("Heartbeat timer error: {}", ec.message());
      return;
    }
    SPDLOG_INFO("Heartbeat timeout, closing WebSocket session");
    close(ws::close_code::normal);
  };

  void do_heartbeat() {
    if (m_wsConfig.pingInterval.count() > 0) {
      do_heartbeat_next();
    }
  };

  void do_heartbeat_next() {
    run_heartbeat_timer(m_wsConfig.pingInterval);
    net::post(m_ws.get_executor(), [this]() {
      erc ec;
      m_ws.ping("ping", ec);
      on_ping(ec);
    });
    // m_ws.async_ping("ping", [this](const erc &ec) { on_ping(ec); });
  };

  void on_ping(const erc &ec) {
    if (ec) {
      SPDLOG_ERROR("Ping error: {}", ec.message());
      close(ws::close_code::policy_error);
    }
  }

private:
  struct WriteMessage {
    bool isBinary{false};
    std::string data{};
  };
  enum class SessionState : uint8_t {
    opening,
    open,
    closing,
    closed,
    shutdown
  };
  std::optional<middleware::auth::AuthenticatedPrincipal> m_authPrincipal{
      std::nullopt};
  http::request<http::string_body> m_request; // 128 bytes
  RequestContext m_requestContext;
  boost::asio::steady_timer m_idleTimer; // 120 bytes
  boost::asio::steady_timer m_heartbeatTimer;
  boost::beast::flat_buffer m_buffer{};                // 48 bytes
  boost::beast::flat_buffer m_httpBuffer{};            // 48 bytes
  std::deque<WriteMessage> m_writeQueue{};             // 40 bytes
  WsConfig m_wsConfig{};                               // 32 bytes
  std::shared_ptr<WsRouter> m_router{nullptr};         // 16 bytes
  boost::beast::websocket::stream<tcp::socket> m_ws{}; // 16 bytes
  std::optional<AuthInfo> m_authInfo{std::nullopt};
  std::shared_ptr<rtl::stp::IExecutor> m_executor{nullptr}; // 16 bytes
  size_t m_currentQueuedBytes{0};
  size_t m_currentQueuedMessages{0};
  SessionState m_curState{SessionState::opening}; // 1 byte
  bool m_writing{false};                          // 1 byte
  bool m_closeNotified{false};                    // 1 byte
};

}; // namespace ws::server
