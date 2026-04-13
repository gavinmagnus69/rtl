#pragma once

#include <functional>
#include <string>

#include "boost/beast/http/status.hpp"
#include <boost/system/error_code.hpp>
#include <string_view>
#include <utility>

#include "Logger.hpp"

namespace ws::server {
using erc = boost::system::error_code;
namespace http = boost::beast::http;
class WsSession;

struct WsAcceptingDecision {
  http::status status{http::status::switching_protocols};
  std::string message{};
  bool accept{true};
  static WsAcceptingDecision make_accept(
      http::status status = http::status::ok, std::string_view msg = "") {
    return WsAcceptingDecision{
        .status = status, .message = std::string(msg), .accept = true};
  };

  static WsAcceptingDecision reject(http::status status,
                                    std::string_view msg = "") {
    return WsAcceptingDecision{
        .status = status, .message = std::string(msg), .accept = false};
  };

  static WsAcceptingDecision default_accept(std::string_view msg = "") {
    return make_accept(http::status::switching_protocols, msg);
  };

  static WsAcceptingDecision accept_ok(std::string_view msg = "OK") {
    return make_accept(http::status::ok, msg);
  };

  static WsAcceptingDecision
  accept_authorized(std::string_view msg = "Authorized") {
    return make_accept(http::status::accepted, msg);
  };

  static WsAcceptingDecision accept_created(std::string_view msg = "Created") {
    return make_accept(http::status::created, msg);
  };

  static WsAcceptingDecision accept_accepted(std::string_view msg = "Accepted") {
    return make_accept(http::status::accepted, msg);
  };

  static WsAcceptingDecision accept_no_content(
      std::string_view msg = "No Content") {
    return make_accept(http::status::no_content, msg);
  };

  static WsAcceptingDecision reject_bad_request(
      std::string_view msg = "Bad Request") {
    return reject(http::status::bad_request, msg);
  };

  static WsAcceptingDecision
  reject_unauthorized(std::string_view msg = "Unauthorized") {
    return reject(http::status::unauthorized, msg);
  };

  static WsAcceptingDecision
  reject_forbidden(std::string_view msg = "Forbidden") {
    return reject(http::status::forbidden, msg);
  };

  static WsAcceptingDecision reject_not_found(std::string_view msg = "Not found") {
    return reject(http::status::not_found, msg);
  };

  static WsAcceptingDecision reject_404(std::string_view msg = "Not found") {
    return reject_not_found(msg);
  };

  static WsAcceptingDecision reject_conflict(std::string_view msg = "Conflict") {
    return reject(http::status::conflict, msg);
  };

  static WsAcceptingDecision reject_payload_too_large(
      std::string_view msg = "Payload Too Large") {
    return reject(http::status::payload_too_large, msg);
  };

  static WsAcceptingDecision reject_too_many_requests(
      std::string_view msg = "Too Many Requests") {
    return reject(http::status::too_many_requests, msg);
  };

  static WsAcceptingDecision reject_internal_server_error(
      std::string_view msg = "Internal Server Error") {
    return reject(http::status::internal_server_error, msg);
  };

  static WsAcceptingDecision reject_not_implemented(
      std::string_view msg = "Not Implemented") {
    return reject(http::status::not_implemented, msg);
  };

  static WsAcceptingDecision reject_service_unavailable(
      std::string_view msg = "Service Unavailable") {
    return reject(http::status::service_unavailable, msg);
  };

  static WsAcceptingDecision reject_503(
      std::string_view msg = "Service Unavailable") {
    return reject_service_unavailable(msg);
  };
};

class WsRouter {
public:
  WsRouter() = default;
  ~WsRouter() = default;

public:
  using OpenHandler = std::function<void(WsSession &)>;
  using CloseHandler = std::function<void(WsSession &)>;
  using ErrorHandler = std::function<void(WsSession &, const erc &)>;
  using MessageHandler = std::function<void(WsSession &, const std::string &)>;
  using AcceptingHandler = std::function<WsAcceptingDecision(WsSession &)>;
  using ProtocolErrorHandler =
      std::function<void(WsSession &, const std::string &)>;
  using AuthHandler = std::function<WsAcceptingDecision(WsSession &)>;
  // calling after auth
  void on_accept(AcceptingHandler handler) {
    m_acceptHandler = std::move(handler);
  }

  void on_open(OpenHandler handler) { m_openHandler = std::move(handler); }

  void on_close(CloseHandler handler) { m_closeHandler = std::move(handler); }

  void on_message(MessageHandler handler) {
    m_messageHandler = std::move(handler);
  }

  void on_error(ErrorHandler handler) { m_errorHandler = std::move(handler); }

  void on_protocol_error(ProtocolErrorHandler handler) {
    m_protoErrHandler = std::move(handler);
  }
  // first connection gate handler (Middleware should be called here)
  void on_auth(AuthHandler handler) { m_authHandler = std::move(handler); };

  void handle_open(WsSession &session) const {
    if (m_openHandler) {
      m_openHandler(session);
    } else {
      SPDLOG_INFO("WebSocket opened: session");
    }
  };

  WsAcceptingDecision handle_accept(WsSession &session) {
    if (m_acceptHandler) {
      return m_acceptHandler(session);
    }
    return WsAcceptingDecision::default_accept();
  };

  void handle_close(WsSession &session) const {
    if (m_closeHandler) {
      m_closeHandler(session);
    } else {
      SPDLOG_INFO("WebSocket closed: session");
    }
  };

  void handle_message(WsSession &session, const std::string &message) const {
    if (m_messageHandler) {
      m_messageHandler(session, message);
    } else {
      SPDLOG_INFO("WebSocket message received: message={}", message);
    }
  };

  void handle_error(WsSession &session, const erc &ec) const {
    if (m_errorHandler) {
      m_errorHandler(session, ec);
    } else {
      SPDLOG_ERROR("WebSocket error: {}", ec.message());
    }
  };

  void handle_protocol_error(WsSession &session, const std::string &msg) {
    if (m_protoErrHandler) {
      m_protoErrHandler(session, msg);
    }
  };

  WsAcceptingDecision handle_auth(WsSession &session) {
    if (m_authHandler) {
      return m_authHandler(session);
    }
    return WsAcceptingDecision::default_accept();
  };

  void handle_binary(WsSession &session) {}

private:
  ErrorHandler m_errorHandler{};
  OpenHandler m_openHandler{};
  CloseHandler m_closeHandler{};
  MessageHandler m_messageHandler{};
  AcceptingHandler m_acceptHandler{};
  ProtocolErrorHandler m_protoErrHandler{};
  AuthHandler m_authHandler{};
};
}; // namespace ws::server
