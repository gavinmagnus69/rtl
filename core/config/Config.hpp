#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>

#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "spdlog/spdlog.h"

// using SettingsReadOnly = ReadOnlyAccessor<T, JsonSerializer<T>>;

namespace rtl::core::config {

using nlohmann::json;

class NotImplementedException final : public std::logic_error {
public:
  NotImplementedException()
      : std::logic_error("Function not yet implemented.") {
  }
  virtual const char* what() const noexcept override {
    return "Function not yet implemented.";
  }
};

class Config {
public:
  // MUST be initialized with path first
  static Config& instance(const std::filesystem::path& path) {
    std::call_once(initFlag_, [local_path = path] { instance_.reset(new Config(local_path)); });
    return *instance_;
  }

  static Config& instance() {
    if (!instance_) {
      throw NotImplementedException();
    }
    return *instance_;
  }

  ~Config() = default;
public:
  const std::string& getServerHost() const noexcept {
    return m_serverHost;
  }
  std::uint16_t getServerPort() const noexcept {
    return m_serverPort;
  }
  std::uint32_t getRequestTimeoutMs() const noexcept {
    return m_requestTimeoutMs;
  }
  std::uint32_t getRequestTimeout() const noexcept {
    return m_requestTimeoutMs;
  }
  std::uint32_t getReadTimeoutMs() const noexcept {
    return m_readTimeoutMs;
  }
  std::uint32_t getWriteTimeoutMs() const noexcept {
    return m_writeTimeoutMs;
  }
  std::uint32_t getMaxConnections() const noexcept {
    return m_maxConnections;
  }

  // get status of config; true - if config initialized successfully, false -
  // otherwise
  bool isOk() const {
    return m_initStatus;
  }
  bool hasKey(const std::string& dottedKey) const noexcept {
    return findNode(dottedKey) != nullptr;
  }
  int32_t getInt(const std::string& dottedKey, int32_t defaultValue = 0) const noexcept {
    const auto* node = findNode(dottedKey);
    if (node && node->is_number_integer()) {
      return node->get<int32_t>();
    }
    return defaultValue;
  };
  bool getBool(const std::string& dottedKey, bool defaultValue = false) const noexcept {
    const auto* node = findNode(dottedKey);
    if (node && node->is_boolean()) {
      return node->get<bool>();
    }
    return defaultValue;
  };
  std::string getString(const std::string& dottedKey, const std::string& defaultValue = "") const noexcept {
    const auto* node = findNode(dottedKey);

    if (node && node->is_string()) {
      return node->get<std::string>();
    }
    SPDLOG_ERROR("Config key '{}' not found or has wrong type", dottedKey);
    return defaultValue;
  };

  nlohmann::json getRaw() const {
    return m_rawConfig;
  };

  // Read-only settings
private:
  Config(const std::filesystem::path& path)
      : m_configFilePath(path) {
    initConfig();
  }

  Config() = delete;
  Config(const Config&) = delete;
  Config& operator=(const Config&) = delete;
  Config& operator=(Config&&) = delete;
  Config(Config&&) = delete;

  void initConfig() {
    if (m_configFilePath.empty()) {
      SPDLOG_ERROR("Config file path is empty");
      m_initStatus = false;
      return;
    }
    if (!std::filesystem::exists(m_configFilePath)) {
      SPDLOG_ERROR("Config file not found: {}", m_configFilePath.string());
      m_initStatus = false;
      return;
    }

    std::ifstream stream(m_configFilePath);
    if (!stream.is_open()) {
      SPDLOG_ERROR("Failed to open config file: {}", m_configFilePath.string());
      m_initStatus = false;
      return;
    }

    try {
      json rawConfig = json::parse(stream);
      m_rawConfig = rawConfig;
      parseServerConfig(rawConfig);
    } catch (const std::exception& ex) {
      SPDLOG_ERROR("Failed to parse config JSON '{}': {}", m_configFilePath.string(), ex.what());
      m_initStatus = false;
      return;
    }

    SPDLOG_INFO("Config initialized from {}", m_configFilePath.string());
    m_initStatus = true;
  }

  // any json usage
  const nlohmann::json* findNode(const std::string& dottedKey) const noexcept {
    if (m_rawConfig.is_null()) {
      return nullptr;
    }
    const nlohmann::json* currentNode = &m_rawConfig;
    std::stringstream ss(dottedKey);
    std::string segment;
    while (std::getline(ss, segment, '.')) {
      SPDLOG_DEBUG("Traversing config node: '{}'", segment);
      if (!currentNode->is_object()) {
        return nullptr;
      }
      auto it = currentNode->find(segment);
      if (it == currentNode->end()) {
        return nullptr;
      }
      currentNode = &(*it);
    }
    return currentNode;
  };

  // concrete server usage
  void parseServerConfig(const json& rawConfig) {
    if (!rawConfig.contains("server") || !rawConfig.at("server").is_object()) {
      SPDLOG_WARN("Missing 'server' section in config. Using defaults.");
      return;
    }

    const auto& server = rawConfig.at("server");
    if (server.contains("host") && server.at("host").is_string()) {
      m_serverHost = server.at("host").get<std::string>();
    }
    if (server.contains("port") && server.at("port").is_number_integer()) {
      const int parsedPort = server.at("port").get<int>();
      if (parsedPort > 0 && parsedPort <= 65535) {
        m_serverPort = static_cast<std::uint16_t>(parsedPort);
      } else {
        SPDLOG_WARN("Invalid server.port={} in config. Using default {}.", parsedPort, m_serverPort);
      }
    }
    if (server.contains("request_timeout_ms") && server.at("request_timeout_ms").is_number_integer()) {
      const auto v = server.at("request_timeout_ms").get<std::int64_t>();
      if (v >= 0) {
        m_requestTimeoutMs = static_cast<std::uint32_t>(v);
      }
    }
    if (server.contains("read_timeout_ms") && server.at("read_timeout_ms").is_number_integer()) {
      const auto v = server.at("read_timeout_ms").get<std::int64_t>();
      if (v >= 0) {
        m_readTimeoutMs = static_cast<std::uint32_t>(v);
      }
    }
    if (server.contains("write_timeout_ms") && server.at("write_timeout_ms").is_number_integer()) {
      const auto v = server.at("write_timeout_ms").get<std::int64_t>();
      if (v >= 0) {
        m_writeTimeoutMs = static_cast<std::uint32_t>(v);
      }
    }
    if (server.contains("max_connections") && server.at("max_connections").is_number_integer()) {
      const auto v = server.at("max_connections").get<std::int64_t>();
      if (v >= 0) {
        m_maxConnections = static_cast<std::uint32_t>(v);
      }
    }
  }

  std::filesystem::path m_configFilePath;
  std::string m_serverHost{"127.0.0.1"};
  std::uint16_t m_serverPort{228};
  std::uint32_t m_requestTimeoutMs{5000};
  std::uint32_t m_readTimeoutMs{5000};
  std::uint32_t m_writeTimeoutMs{5000};
  std::uint32_t m_maxConnections{1024};
  nlohmann::json m_rawConfig;
  bool m_initStatus{false};
  static inline std::unique_ptr<Config> instance_;
  static inline std::once_flag initFlag_;
};

}; // namespace rtl::core::config
