#pragma once
#include "IAuthenticator.hpp"
#include <optional>
#include <string>
#include <variant>

namespace middleware::auth {

struct AuthorizationRequest {
  std::string action{};
  std::string endpoint{};
  std::optional<std::string> resource_type{std::nullopt};
  std::optional<std::string> resource_id{std::nullopt};
};

struct AuthorizationSuccess {};

enum class AuthorizationFailureCode {
  insufficient_role,
  insufficient_scope,
  forbidden,
  temporarily_unavailable,
  expired_token,
  unknown_request,
  unknown_resource
};

[[nodiscard]] inline std::string
authorization_failure_code_to_string(AuthorizationFailureCode code) {
  switch (code) {
  case AuthorizationFailureCode::insufficient_role:
    return "insufficient_role";
  case AuthorizationFailureCode::insufficient_scope:
    return "insufficient_scope";
  case AuthorizationFailureCode::forbidden:
    return "forbidden";
  case AuthorizationFailureCode::temporarily_unavailable:
    return "temporarily_unavailable";
  case AuthorizationFailureCode::expired_token:
    return "expired_token";
  case AuthorizationFailureCode::unknown_request:
    return "unknown_request";
  case AuthorizationFailureCode::unknown_resource:
    return "unknown_resource";
  }

  return "unknown";
};

struct AuthorizationFailure {
  AuthorizationFailureCode code;
  std::string message{};
  bool retryable{false};
};

using AuthorizationResult =
    std::variant<AuthorizationSuccess, AuthorizationFailure>;

struct IAuthorizer {
  virtual ~IAuthorizer() = default;

  [[nodiscard]] virtual AuthorizationResult
  authorize(const AuthenticatedPrincipal &,
            const AuthorizationRequest &) const = 0;
};
}; // namespace middleware::auth
