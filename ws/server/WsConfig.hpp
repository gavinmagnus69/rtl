#pragma once

#include "Config.hpp"
#include <chrono>

namespace ws::server {
struct WsConfig {

  /// Maximum accepted payload size in bytes (soft limit).
  std::size_t maxMessageSize = 64 * 1024; // 64 KiB
  std::size_t maxQueuedBytes = 10 * 1024; // 10 KiB
  std::size_t maxQueuedMessages = 256;

  /// Idle timeout after which an inactive connection is closed.
  std::chrono::seconds idleTimeout{60};
  // Drain timeout after which all active unclosed sessions forcefully closing
  std::chrono::seconds drainTimeout{5};
  /// Enable permessage-deflate compression if client supports it.
  bool enablePerMessageDeflate = true;
  /// Allow automatic ping/pong management by Beast.
  bool autoPingPong = true;
  /// Interval between server-initiated pings (0 = disabled).
  std::chrono::seconds pingInterval{30};

  static WsConfig from_core(const core::config::Config &core) {
    WsConfig config;
    config.maxMessageSize =
        core.getInt("websocket.max_message_size", config.maxMessageSize);
    config.maxQueuedBytes =
        core.getInt("websocket.max_queued_bytes", config.maxQueuedBytes);
    config.maxQueuedMessages =
        core.getInt("websocket.max_queued_messages", config.maxQueuedMessages);
    config.idleTimeout = std::chrono::seconds(
        core.getInt("websocket.idle_timeout", config.idleTimeout.count()));
    config.drainTimeout = std::chrono::seconds(
        core.getInt("websocket.drain_timeout", config.drainTimeout.count()));
    config.enablePerMessageDeflate = core.getBool(
        "websocket.enable_deflate", config.enablePerMessageDeflate);
    config.pingInterval = std::chrono::seconds(
        core.getInt("websocket.ping_interval", config.pingInterval.count()));
    return config;
  };

  static WsConfig from_file(const std::filesystem::path &path) {
    auto &coreConfig = core::config::Config::instance(path);
    if (!coreConfig.isOk()) {
      SPDLOG_ERROR("Failed to load core config from file: {}", path.string());
      return {};
    }
    return from_core(coreConfig);
  };
};
}; // namespace ws::server