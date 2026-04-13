#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "GetDeviceCapabilitiesRequestDto.hpp"
#include "GetDeviceStateRequestDto.hpp"
#include "ListDevicesRequestDto.hpp"
#include "SetDeviceParameterRequestDto.hpp"
#include "StartConnectRequestDto.hpp"
#include "StartDisconnectRequestDto.hpp"
#include "StartStreamRequestDto.hpp"
#include "StopStreamRequestDto.hpp"
#include "SubscribeTelemetryRequestDto.hpp"
#include "UnsubscribeTelemetryRequestDto.hpp"
#include "WsRequestContextDto.hpp"
#include "WsRequestDtoVariant.hpp"
#include "WsProtocol.hpp"

namespace ws::dto {

struct ParsedWsRequestDto {
  WsRequestContextDto context{};
  WsRequestDtoVariant request{};

  [[nodiscard]] bool is_valid() const noexcept { return context.is_valid(); }
};

namespace detail {

using json = ws::server::json;
using JsonMessage = ws::server::JsonMessage;

template <typename T>
[[nodiscard]] inline std::optional<T>
read_string_field(const json &object, std::string_view key);

template <>
[[nodiscard]] inline std::optional<std::string>
read_string_field<std::string>(const json &object, std::string_view key) {
  const auto it = object.find(key);
  if (it == object.end() || !it->is_string()) {
    return std::nullopt;
  }
  return it->get<std::string>();
}

[[nodiscard]] inline std::optional<bool>
read_bool_field(const json &object, std::string_view key) {
  const auto it = object.find(key);
  if (it == object.end() || !it->is_boolean()) {
    return std::nullopt;
  }
  return it->get<bool>();
}

template <typename UInt>
[[nodiscard]] inline std::optional<UInt>
read_unsigned_field(const json &object, std::string_view key) {
  const auto it = object.find(key);
  if (it == object.end() || !it->is_number_unsigned()) {
    return std::nullopt;
  }
  return it->get<UInt>();
}

[[nodiscard]] inline std::optional<std::vector<std::string>>
read_string_array_field(const json &object, std::string_view key) {
  const auto it = object.find(key);
  if (it == object.end() || !it->is_array()) {
    return std::nullopt;
  }

  std::vector<std::string> values;
  values.reserve(it->size());
  for (const auto &item : *it) {
    if (!item.is_string()) {
      return std::nullopt;
    }
    values.push_back(item.get<std::string>());
  }
  return values;
}

[[nodiscard]] inline std::string
read_optional_string_or_empty(const json &object, std::string_view key) {
  const auto value = read_string_field<std::string>(object, key);
  return value.value_or(std::string{});
}

[[nodiscard]] inline bool
read_optional_bool(const json &object, std::string_view key, bool fallback) {
  const auto value = read_bool_field(object, key);
  return value.value_or(fallback);
}

template <typename UInt>
[[nodiscard]] inline std::optional<UInt>
read_optional_unsigned(const json &object, std::string_view key) {
  const auto it = object.find(key);
  if (it == object.end()) {
    return std::nullopt;
  }
  if (!it->is_number_unsigned()) {
    return std::nullopt;
  }
  return it->get<UInt>();
}

[[nodiscard]] inline std::vector<std::string>
read_optional_string_array(const json &object, std::string_view key) {
  const auto value = read_string_array_field(object, key);
  return value.value_or(std::vector<std::string>{});
}

[[nodiscard]] inline std::string
read_meta_string(const json &meta, std::string_view key) {
  if (!meta.is_object()) {
    return {};
  }
  return read_optional_string_or_empty(meta, key);
}

[[nodiscard]] inline json make_context_meta(const WsRequestContextDto &context) {
  json meta = json::object();
  if (!context.session_id.empty()) {
    meta["session_id"] = context.session_id;
  }
  if (!context.user_id.empty()) {
    meta["user_id"] = context.user_id;
  }
  return meta;
}

[[nodiscard]] inline std::optional<json>
request_payload(const JsonMessage &message) {
  if (!message.payload.is_object()) {
    return std::nullopt;
  }
  return message.payload;
}

template <typename T>
[[nodiscard]] inline std::optional<WsRequestDtoVariant>
as_variant(std::optional<T> value) {
  if (!value.has_value()) {
    return std::nullopt;
  }
  return WsRequestDtoVariant{std::move(*value)};
}

} // namespace detail

[[nodiscard]] inline WsRequestContextDto
json_message_to_request_context(const ws::server::JsonMessage &message) {
  WsRequestContextDto context{};
  context.request_id = message.id;
  context.correlation_id = message.correlation_id;
  context.method = message.type;
  context.version = message.version;
  context.user_id = message.sender;

  if (message.meta.is_object()) {
    const auto meta_user_id =
        detail::read_string_field<std::string>(message.meta, "user_id");
    if (meta_user_id.has_value()) {
      context.user_id = std::move(*meta_user_id);
    }

    const auto session_id =
        detail::read_string_field<std::string>(message.meta, "session_id");
    if (session_id.has_value()) {
      context.session_id = std::move(*session_id);
    }
  }

  return context;
}

[[nodiscard]] inline ws::server::JsonMessage
request_context_to_json_message(const WsRequestContextDto &context) {
  ws::server::JsonMessage message{};
  message.id = context.request_id;
  message.correlation_id = context.correlation_id;
  message.kind = "request";
  message.type = context.method;
  message.sender = context.user_id;
  message.version = context.version;
  message.payload = ws::server::json::object();
  message.meta = detail::make_context_meta(context);
  return message;
}

[[nodiscard]] inline std::optional<ListDevicesRequestDto>
json_message_to_dto_list_devices(const ws::server::JsonMessage &message) {
  const auto payload = detail::request_payload(message);
  if (!payload.has_value()) {
    return std::nullopt;
  }

  ListDevicesRequestDto dto{};
  dto.include_disconnected =
      detail::read_optional_bool(*payload, "include_disconnected", true);
  dto.include_capabilities =
      detail::read_optional_bool(*payload, "include_capabilities", false);
  return dto;
}

[[nodiscard]] inline ws::server::JsonMessage
dto_to_json_message(const ListDevicesRequestDto &dto,
                    const WsRequestContextDto &context) {
  auto message = request_context_to_json_message(context);
  message.type = std::string{ListDevicesRequestDto::method_name};
  message.payload = {{"include_disconnected", dto.include_disconnected},
                     {"include_capabilities", dto.include_capabilities}};
  return message;
}

[[nodiscard]] inline std::optional<GetDeviceStateRequestDto>
json_message_to_dto_get_device_state(const ws::server::JsonMessage &message) {
  const auto payload = detail::request_payload(message);
  if (!payload.has_value()) {
    return std::nullopt;
  }

  const auto device_id =
      detail::read_string_field<std::string>(*payload, "device_id");
  if (!device_id.has_value()) {
    return std::nullopt;
  }

  GetDeviceStateRequestDto dto{};
  dto.device_id = std::move(*device_id);
  dto.include_capabilities =
      detail::read_optional_bool(*payload, "include_capabilities", false);
  if (!dto.is_valid()) {
    return std::nullopt;
  }
  return dto;
}

[[nodiscard]] inline ws::server::JsonMessage
dto_to_json_message(const GetDeviceStateRequestDto &dto,
                    const WsRequestContextDto &context) {
  auto message = request_context_to_json_message(context);
  message.type = std::string{GetDeviceStateRequestDto::method_name};
  message.payload = {{"device_id", dto.device_id},
                     {"include_capabilities", dto.include_capabilities}};
  return message;
}

[[nodiscard]] inline std::optional<GetDeviceCapabilitiesRequestDto>
json_message_to_dto_get_device_capabilities(
    const ws::server::JsonMessage &message) {
  const auto payload = detail::request_payload(message);
  if (!payload.has_value()) {
    return std::nullopt;
  }

  const auto device_id =
      detail::read_string_field<std::string>(*payload, "device_id");
  if (!device_id.has_value()) {
    return std::nullopt;
  }

  GetDeviceCapabilitiesRequestDto dto{};
  dto.device_id = std::move(*device_id);
  if (!dto.is_valid()) {
    return std::nullopt;
  }
  return dto;
}

[[nodiscard]] inline ws::server::JsonMessage
dto_to_json_message(const GetDeviceCapabilitiesRequestDto &dto,
                    const WsRequestContextDto &context) {
  auto message = request_context_to_json_message(context);
  message.type = std::string{GetDeviceCapabilitiesRequestDto::method_name};
  message.payload = {{"device_id", dto.device_id}};
  return message;
}

[[nodiscard]] inline std::optional<StartConnectRequestDto>
json_message_to_dto_start_connect(const ws::server::JsonMessage &message) {
  const auto payload = detail::request_payload(message);
  if (!payload.has_value()) {
    return std::nullopt;
  }

  const auto device_id =
      detail::read_string_field<std::string>(*payload, "device_id");
  if (!device_id.has_value()) {
    return std::nullopt;
  }

  StartConnectRequestDto dto{};
  dto.device_id = std::move(*device_id);
  dto.force_reconnect =
      detail::read_optional_bool(*payload, "force_reconnect", false);
  dto.timeout_ms =
      detail::read_optional_unsigned<std::uint32_t>(*payload, "timeout_ms");
  if (!dto.is_valid()) {
    return std::nullopt;
  }
  return dto;
}

[[nodiscard]] inline ws::server::JsonMessage
dto_to_json_message(const StartConnectRequestDto &dto,
                    const WsRequestContextDto &context) {
  auto message = request_context_to_json_message(context);
  message.type = std::string{StartConnectRequestDto::method_name};
  message.payload = {{"device_id", dto.device_id},
                     {"force_reconnect", dto.force_reconnect}};
  if (dto.timeout_ms.has_value()) {
    message.payload["timeout_ms"] = *dto.timeout_ms;
  }
  return message;
}

[[nodiscard]] inline std::optional<StartDisconnectRequestDto>
json_message_to_dto_start_disconnect(const ws::server::JsonMessage &message) {
  const auto payload = detail::request_payload(message);
  if (!payload.has_value()) {
    return std::nullopt;
  }

  const auto device_id =
      detail::read_string_field<std::string>(*payload, "device_id");
  if (!device_id.has_value()) {
    return std::nullopt;
  }

  StartDisconnectRequestDto dto{};
  dto.device_id = std::move(*device_id);
  dto.force = detail::read_optional_bool(*payload, "force", false);
  dto.reason = detail::read_optional_string_or_empty(*payload, "reason");
  if (!dto.is_valid()) {
    return std::nullopt;
  }
  return dto;
}

[[nodiscard]] inline ws::server::JsonMessage
dto_to_json_message(const StartDisconnectRequestDto &dto,
                    const WsRequestContextDto &context) {
  auto message = request_context_to_json_message(context);
  message.type = std::string{StartDisconnectRequestDto::method_name};
  message.payload = {{"device_id", dto.device_id}, {"force", dto.force}};
  if (!dto.reason.empty()) {
    message.payload["reason"] = dto.reason;
  }
  return message;
}

[[nodiscard]] inline std::optional<StartStreamRequestDto>
json_message_to_dto_start_stream(const ws::server::JsonMessage &message) {
  const auto payload = detail::request_payload(message);
  if (!payload.has_value()) {
    return std::nullopt;
  }

  const auto device_id =
      detail::read_string_field<std::string>(*payload, "device_id");
  const auto stream_name =
      detail::read_string_field<std::string>(*payload, "stream_name");
  if (!device_id.has_value() || !stream_name.has_value()) {
    return std::nullopt;
  }

  StartStreamRequestDto dto{};
  dto.device_id = std::move(*device_id);
  dto.stream_name = std::move(*stream_name);
  dto.frame_rate_hz =
      detail::read_optional_unsigned<std::uint32_t>(*payload, "frame_rate_hz");
  dto.sample_rate_hz =
      detail::read_optional_unsigned<std::uint32_t>(*payload, "sample_rate_hz");
  dto.batch_size =
      detail::read_optional_unsigned<std::uint32_t>(*payload, "batch_size");
  dto.replay_last_value =
      detail::read_optional_bool(*payload, "replay_last_value", false);
  if (!dto.is_valid()) {
    return std::nullopt;
  }
  return dto;
}

[[nodiscard]] inline ws::server::JsonMessage
dto_to_json_message(const StartStreamRequestDto &dto,
                    const WsRequestContextDto &context) {
  auto message = request_context_to_json_message(context);
  message.type = std::string{StartStreamRequestDto::method_name};
  message.payload = {{"device_id", dto.device_id},
                     {"stream_name", dto.stream_name},
                     {"replay_last_value", dto.replay_last_value}};
  if (dto.frame_rate_hz.has_value()) {
    message.payload["frame_rate_hz"] = *dto.frame_rate_hz;
  }
  if (dto.sample_rate_hz.has_value()) {
    message.payload["sample_rate_hz"] = *dto.sample_rate_hz;
  }
  if (dto.batch_size.has_value()) {
    message.payload["batch_size"] = *dto.batch_size;
  }
  return message;
}

[[nodiscard]] inline std::optional<StopStreamRequestDto>
json_message_to_dto_stop_stream(const ws::server::JsonMessage &message) {
  const auto payload = detail::request_payload(message);
  if (!payload.has_value()) {
    return std::nullopt;
  }

  const auto device_id =
      detail::read_string_field<std::string>(*payload, "device_id");
  const auto stream_name =
      detail::read_string_field<std::string>(*payload, "stream_name");
  if (!device_id.has_value() || !stream_name.has_value()) {
    return std::nullopt;
  }

  StopStreamRequestDto dto{};
  dto.device_id = std::move(*device_id);
  dto.stream_name = std::move(*stream_name);
  dto.reason = detail::read_optional_string_or_empty(*payload, "reason");
  if (!dto.is_valid()) {
    return std::nullopt;
  }
  return dto;
}

[[nodiscard]] inline ws::server::JsonMessage
dto_to_json_message(const StopStreamRequestDto &dto,
                    const WsRequestContextDto &context) {
  auto message = request_context_to_json_message(context);
  message.type = std::string{StopStreamRequestDto::method_name};
  message.payload = {{"device_id", dto.device_id},
                     {"stream_name", dto.stream_name}};
  if (!dto.reason.empty()) {
    message.payload["reason"] = dto.reason;
  }
  return message;
}

[[nodiscard]] inline std::optional<SubscribeTelemetryRequestDto>
json_message_to_dto_subscribe_telemetry(const ws::server::JsonMessage &message) {
  const auto payload = detail::request_payload(message);
  if (!payload.has_value()) {
    return std::nullopt;
  }

  const auto device_id =
      detail::read_string_field<std::string>(*payload, "device_id");
  const auto topics = detail::read_string_array_field(*payload, "topics");
  if (!device_id.has_value() || !topics.has_value()) {
    return std::nullopt;
  }

  SubscribeTelemetryRequestDto dto{};
  dto.device_id = std::move(*device_id);
  dto.topics = std::move(*topics);
  dto.include_snapshot =
      detail::read_optional_bool(*payload, "include_snapshot", true);
  if (!dto.is_valid()) {
    return std::nullopt;
  }
  return dto;
}

[[nodiscard]] inline ws::server::JsonMessage
dto_to_json_message(const SubscribeTelemetryRequestDto &dto,
                    const WsRequestContextDto &context) {
  auto message = request_context_to_json_message(context);
  message.type = std::string{SubscribeTelemetryRequestDto::method_name};
  message.payload = {{"device_id", dto.device_id},
                     {"topics", dto.topics},
                     {"include_snapshot", dto.include_snapshot}};
  return message;
}

[[nodiscard]] inline std::optional<UnsubscribeTelemetryRequestDto>
json_message_to_dto_unsubscribe_telemetry(
    const ws::server::JsonMessage &message) {
  const auto payload = detail::request_payload(message);
  if (!payload.has_value()) {
    return std::nullopt;
  }

  const auto device_id =
      detail::read_string_field<std::string>(*payload, "device_id");
  const auto topics = detail::read_string_array_field(*payload, "topics");
  if (!device_id.has_value() || !topics.has_value()) {
    return std::nullopt;
  }

  UnsubscribeTelemetryRequestDto dto{};
  dto.device_id = std::move(*device_id);
  dto.topics = std::move(*topics);
  if (!dto.is_valid()) {
    return std::nullopt;
  }
  return dto;
}

[[nodiscard]] inline ws::server::JsonMessage
dto_to_json_message(const UnsubscribeTelemetryRequestDto &dto,
                    const WsRequestContextDto &context) {
  auto message = request_context_to_json_message(context);
  message.type = std::string{UnsubscribeTelemetryRequestDto::method_name};
  message.payload = {{"device_id", dto.device_id}, {"topics", dto.topics}};
  return message;
}

[[nodiscard]] inline std::optional<SetDeviceParameterRequestDto>
json_message_to_dto_set_device_parameter(
    const ws::server::JsonMessage &message) {
  const auto payload = detail::request_payload(message);
  if (!payload.has_value()) {
    return std::nullopt;
  }

  const auto device_id =
      detail::read_string_field<std::string>(*payload, "device_id");
  const auto parameter_name =
      detail::read_string_field<std::string>(*payload, "parameter_name");
  if (!device_id.has_value() || !parameter_name.has_value()) {
    return std::nullopt;
  }

  SetDeviceParameterRequestDto dto{};
  dto.device_id = std::move(*device_id);
  dto.parameter_name = std::move(*parameter_name);
  dto.value = detail::read_optional_string_or_empty(*payload, "value");
  if (!dto.is_valid()) {
    return std::nullopt;
  }
  return dto;
}

[[nodiscard]] inline ws::server::JsonMessage
dto_to_json_message(const SetDeviceParameterRequestDto &dto,
                    const WsRequestContextDto &context) {
  auto message = request_context_to_json_message(context);
  message.type = std::string{SetDeviceParameterRequestDto::method_name};
  message.payload = {{"device_id", dto.device_id},
                     {"parameter_name", dto.parameter_name},
                     {"value", dto.value}};
  return message;
}

[[nodiscard]] inline std::optional<WsRequestDtoVariant>
json_message_to_request_dto_variant(const ws::server::JsonMessage &message) {
  if (message.type == ListDevicesRequestDto::method_name) {
    return detail::as_variant(json_message_to_dto_list_devices(message));
  }
  if (message.type == GetDeviceStateRequestDto::method_name) {
    return detail::as_variant(json_message_to_dto_get_device_state(message));
  }
  if (message.type == GetDeviceCapabilitiesRequestDto::method_name) {
    return detail::as_variant(
        json_message_to_dto_get_device_capabilities(message));
  }
  if (message.type == StartConnectRequestDto::method_name) {
    return detail::as_variant(json_message_to_dto_start_connect(message));
  }
  if (message.type == StartDisconnectRequestDto::method_name) {
    return detail::as_variant(json_message_to_dto_start_disconnect(message));
  }
  if (message.type == StartStreamRequestDto::method_name) {
    return detail::as_variant(json_message_to_dto_start_stream(message));
  }
  if (message.type == StopStreamRequestDto::method_name) {
    return detail::as_variant(json_message_to_dto_stop_stream(message));
  }
  if (message.type == SubscribeTelemetryRequestDto::method_name) {
    return detail::as_variant(
        json_message_to_dto_subscribe_telemetry(message));
  }
  if (message.type == UnsubscribeTelemetryRequestDto::method_name) {
    return detail::as_variant(
        json_message_to_dto_unsubscribe_telemetry(message));
  }
  if (message.type == SetDeviceParameterRequestDto::method_name) {
    return detail::as_variant(
        json_message_to_dto_set_device_parameter(message));
  }
  return std::nullopt;
}

[[nodiscard]] inline std::optional<ParsedWsRequestDto>
json_message_to_request_dto(const ws::server::JsonMessage &message) {
  auto request = json_message_to_request_dto_variant(message);
  if (!request.has_value()) {
    return std::nullopt;
  }

  ParsedWsRequestDto parsed{};
  parsed.context = json_message_to_request_context(message);
  parsed.request = std::move(*request);
  if (!parsed.context.is_valid()) {
    return std::nullopt;
  }
  return parsed;
}

[[nodiscard]] inline ws::server::JsonMessage
request_dto_to_json_message(const WsRequestDtoVariant &request,
                            const WsRequestContextDto &context) {
  return std::visit(
      [&context](const auto &dto) { return dto_to_json_message(dto, context); },
      request);
}

[[nodiscard]] inline ws::server::JsonMessage
request_dto_to_json_message(const ParsedWsRequestDto &parsed) {
  return request_dto_to_json_message(parsed.request, parsed.context);
}

} // namespace ws::dto
