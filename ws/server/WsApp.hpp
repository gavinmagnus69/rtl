#pragma once

#include <format>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>

#include "Config.hpp"
#include "WsConfig.hpp"
#include "WsProtocol.hpp"
#include "WsRouter.hpp"
#include "WsServer.hpp"
#include "WsSession.hpp"

#include "IAuthenticator.hpp"

#include "boost/beast/http/status.hpp"

#include "spdlog/spdlog.h"

namespace ws::server {

using EndpointHandler = std::function<void(WsSession &, const JsonMessage &)>;
using AuthenticationRequest = middleware::auth::AuthenticationRequest;

[[nodiscard]] AuthenticationRequest
session_context_to_authentication_request(WsSession &session) {
  AuthenticationRequest request;
  const auto &ctx = session.request_context();

  request.normalized_headers = ctx.get_headers();
  request.endpoint = session.endpoint();
  request.remote_address = ctx.remote_address();
  request.remote_port = ctx.remote_port();
  request.raw_target = ctx.target();
  request.query_string = ctx.query_string();
  if (auto it = request.normalized_headers.find("authorization");
      it != request.normalized_headers.end()) {
    request.authorization_header = it->second;
  }
  if (auto it = request.normalized_headers.find("host");
      it != request.normalized_headers.end()) {
    request.host = it->second;
  }

  if (auto it = request.normalized_headers.find("origin");
      it != request.normalized_headers.end()) {
    request.origin = it->second;
  }

  if (auto it = request.normalized_headers.find("user-agent");
      it != request.normalized_headers.end()) {
    request.user_agent = it->second;
  }
  // TODO: parse cookies and query_params
  return request;
};

WsAcceptingDecision
reject_on_auth_failure(const middleware::auth::AuthenticationFailure &fail) {
  const auto message_or = [&fail](std::string_view fallback) {
    if (fail.message.empty()) {
      return fallback;
    }
    return std::string_view{fail.message};
  };

  switch (fail.code) {
  case middleware::auth::AuthenticationFailureCode::forbidden:
    return WsAcceptingDecision::reject_forbidden(message_or("Forbidden"));
  case middleware::auth::AuthenticationFailureCode::temporarily_unavailable:
    return WsAcceptingDecision::reject_service_unavailable(
        message_or("Service Unavailable"));
  case middleware::auth::AuthenticationFailureCode::missing_credentials:
  case middleware::auth::AuthenticationFailureCode::malformed_credentials:
  case middleware::auth::AuthenticationFailureCode::invalid_token:
  case middleware::auth::AuthenticationFailureCode::expired_token:
  case middleware::auth::AuthenticationFailureCode::invalid_issuer:
  case middleware::auth::AuthenticationFailureCode::invalid_audience:
    return WsAcceptingDecision::reject_unauthorized(message_or("Unauthorized"));
  }

  return WsAcceptingDecision::reject_unauthorized(message_or("Unauthorized"));
};

class WsApp {
public:
  explicit WsApp(const std::string &config_path)
      : m_coreConfig(core::config::Config::instance(config_path)),
        m_server(core::config::Config::instance(config_path), nullptr) {
    update_endpoint_routing();
  };
  ~WsApp() = default;
  void run() { m_server.start(); }
  void run_blocking() { m_server.start_blocking(); };
  void stop() { m_server.stop(); };
  WsApp &add_endpoint(const std::string &endpoint, EndpointHandler handler) {
    if (endpoint.empty()) {
      SPDLOG_WARN("Empty endpoint name");
      return *this;
    }
    if (!handler) {
      SPDLOG_WARN("Empty handler for {}", endpoint);
      return *this;
    }
    if (m_endpHndlMap.contains(endpoint)) {
      SPDLOG_WARN("Endpoint handler already exists");
      return *this;
    }
    m_endpHndlMap.emplace(endpoint, std::move(handler));
    update_endpoint_routing();
    return *this;
  };

  WsApp &add_message_handler(std::string_view endpoint, std::string_view type,
                             EndpointHandler handler) {
    if (endpoint.empty()) {
      SPDLOG_WARN("Empty endpoint name");
      return *this;
    }
    if (type.empty()) {
      SPDLOG_WARN("Empty type name");
      return *this;
    }
    if (!handler) {
      SPDLOG_WARN("Empty handler for {}.{}", endpoint, type);
      return *this;
    }
    std::string endpoint_string{endpoint};
    if (!m_nestedHdnlMap.contains(endpoint_string)) {
      m_nestedHdnlMap.emplace(
          endpoint, std::unordered_map<std::string, EndpointHandler>{});
    }
    std::string type_string{type};
    if (m_nestedHdnlMap.at(endpoint_string).contains(type_string)) {
      SPDLOG_WARN("Handler already exists");
      return *this;
    }
    m_nestedHdnlMap.at(endpoint_string)
        .emplace(type_string, std::move(handler));
    update_endpoint_routing();
    return *this;
  };

  void setup_middleware(
      std::shared_ptr<middleware::auth::IAuthenticator> middleware) {
    m_middleware = std::move(middleware);
  };

private:
  void update_endpoint_routing() {
    setup_error();
    setup_protocol_error();
    setup_accepting();
    setup_auth();
    setup_messaging();
  };

  void setup_accepting() {
    m_server.on_accept([this](WsSession &session) {
      const std::string &endpoint = session.endpoint();
      auto it = m_endpHndlMap.find(endpoint);
      auto it_nested = m_nestedHdnlMap.find(endpoint);
      if (it == m_endpHndlMap.end() && it_nested == m_nestedHdnlMap.end()) {
        return WsAcceptingDecision{.status = http::status::not_found,
                                   .message = "Unknown endpoint",
                                   .accept = false};
      }
      SPDLOG_INFO("Accepted new websocket session: {}, request query: {}",
                  endpoint, session.query_string());
      return WsAcceptingDecision::default_accept();
    });
  };

  void setup_error() {
    m_server.on_error([this](WsSession &session, const erc &ec) {
      SPDLOG_ERROR("Error {}", ec.message());
    });
  };

  void setup_protocol_error() {
    m_server.on_protocol_error(
        [this](WsSession &session, const std::string &msg) {
          SPDLOG_ERROR("Protocol error {}", msg);
        });
  };

  void setup_auth() {
    m_server.on_auth([this](WsSession &session) {
      const auto &req_ctx = session.request_context();
      SPDLOG_INFO("remote={}:{} ts={}", req_ctx.remote_address(),
                  req_ctx.remote_port(), req_ctx.connect_timestamp());

      if (m_middleware) {

        auto request_details =
            session_context_to_authentication_request(session);
        auto auth_result = m_middleware->authenticate(request_details);
        if (std::holds_alternative<middleware::auth::AuthenticatedPrincipal>(
                auth_result)) {
          auto principal =
              std::get<middleware::auth::AuthenticatedPrincipal>(auth_result);
          SPDLOG_INFO("Auth success: user_id={}, issuer={}, ",
                      principal.user_id, principal.issuer);

          session.set_authentication_principal(principal);
          return WsAcceptingDecision::accept_authorized();
        };
        if (std::holds_alternative<middleware::auth::AuthenticationFailure>(
                auth_result)) {
          auto rejection =
              std::get<middleware::auth::AuthenticationFailure>(auth_result);
          SPDLOG_ERROR("Auth failure: code={}, message={}",
                       (size_t)rejection.code, rejection.message);
          return reject_on_auth_failure(rejection);
        };
        return WsAcceptingDecision::reject_unauthorized();
      }
      return WsAcceptingDecision::reject_unauthorized();
    });
  };

  void setup_messaging() {
    m_server.on_typed_message(
        [this](WsSession &session, const JsonMessage &message) {
          const std::string &endpoint = session.endpoint();
          const std::string &type = message.type;
          if (!type.empty()) {
            auto it_nested = m_nestedHdnlMap.find(endpoint);
            if (it_nested == m_nestedHdnlMap.end()) {
              session.close();
              return;
            };
            auto it_nested_handler = it_nested->second.find(type);
            if (it_nested_handler == it_nested->second.end()) {
              SPDLOG_INFO("Unknown message type: {}/{}", endpoint, type);
              return;
            }
            it_nested_handler->second(session, message);
            return;
          }
          auto it = m_endpHndlMap.find(session.endpoint());
          if (it == m_endpHndlMap.end()) {
            session.close();
            return;
          }
          it->second(session, message);
        });
  };

  core::config::Config &m_coreConfig;
  WsConfig m_config;
  std::shared_ptr<middleware::auth::IAuthenticator> m_middleware{nullptr};
  std::unordered_map<std::string, EndpointHandler> m_endpHndlMap;
  std::unordered_map<std::string,
                     std::unordered_map<std::string, EndpointHandler>>
      m_nestedHdnlMap;
  WsServer m_server;
};
}; // namespace ws::server
