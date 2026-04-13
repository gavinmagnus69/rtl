#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace middleware::auth {

struct AuthenticationRequest {
  std::optional<std::string> authorization_header{std::nullopt};
  std::unordered_map<std::string, std::string> normalized_headers{};
  std::unordered_map<std::string, std::string> query_params{};
  std::unordered_map<std::string, std::string> cookies{};

  std::string endpoint{};
  std::string raw_target{};
  std::string query_string{};

  std::string remote_address{};
  uint16_t remote_port{};

  std::string host{};
  std::string origin{};
  std::string user_agent{};
};

struct AuthenticatedPrincipal {
  std::string user_id{};
  std::string auth_scheme{};
  std::string subject{};
  std::string issuer{};
  std::vector<std::string> roles{};
  std::vector<std::string> scopes{};
  std::optional<std::chrono::system_clock::time_point> expires_at{std::nullopt};
};

enum class AuthenticationFailureCode {
  missing_credentials,
  malformed_credentials,
  invalid_token,
  expired_token,
  invalid_issuer,
  invalid_audience,
  forbidden,
  temporarily_unavailable
};

struct AuthenticationFailure {
  AuthenticationFailureCode code;
  std::string message{};
  bool retryable{false};
};

using AuthenticationResult =
    std::variant<AuthenticatedPrincipal, AuthenticationFailure>;

struct IAuthenticator {
  virtual ~IAuthenticator() = default;
  [[nodiscard]] virtual AuthenticationResult
  authenticate(const AuthenticationRequest &request) const = 0;
};
}; // namespace middleware::auth