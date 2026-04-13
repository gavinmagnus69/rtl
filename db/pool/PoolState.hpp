#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

#include "Connection.hpp"
#include "ConnectionConfig.hpp"

#include "Errors.hpp"
#include "PoolConfig.hpp"

namespace db::pool {

struct PoolState {
  PoolState(const PoolConfig &pool_cfg,
            const db::core::ConnectionConfig &con_cfg)
      : config(pool_cfg), connection_config(con_cfg) {
    if (config.max_connections == 0) {
      throw db::core::ConfigurationError{
          "Invalid max_connections param in config"};
    }
    connections.reserve(config.max_connections);
  };

  ~PoolState() { shutdown(); };

  void return_connection(std::unique_ptr<db::core::Connection> connection) {
    std::lock_guard lock(m_mtx);
    --active_connections;
    if (m_shutdown) {
      m_cond.notify_all();
      return;
    }
    if (!connection) {
      m_cond.notify_one();
      return;
    }
    if (!connection->is_open()) {
      m_cond.notify_one();
      return;
    }
    connections.push_back(std::move(connection));
    ++idle_connections;
    m_cond.notify_one();
  }

  std::unique_ptr<db::core::Connection> acquire() {
    std::unique_lock lock(m_mtx);
    if (m_shutdown) {
      m_cond.notify_all();
      throw db::core::PoolExhausted("Connection pool is shutting down");
    }
    auto connection = take_connection_locked();
    if (connection) {
      return connection;
    }
    m_cond.wait(lock, [this]() {
      return m_shutdown || !connections.empty() ||
             can_create_connection_locked();
    });
    if (m_shutdown) {
      m_cond.notify_all();
      throw db::core::PoolExhausted("Connection pool is shutting down");
    }
    return take_connection_locked();
  }

  std::unique_ptr<db::core::Connection>
  try_acquire_for(std::chrono::milliseconds timeout) {
    std::unique_lock lock(m_mtx);
    if (m_shutdown) {
      throw db::core::PoolExhausted("Connection pool is shutting down");
    }
    auto connection = take_connection_locked();
    if (connection) {
      return connection;
    }
    const auto result = m_cond.wait_for(lock, timeout, [this]() {
      return m_shutdown || !connections.empty() ||
             can_create_connection_locked();
    });
    if (!result) {
      throw db::core::PoolTimeout("Timed out waiting for a pooled connection");
    }
    if (m_shutdown) {
      throw db::core::PoolExhausted("Connection pool is shutting down");
    }
    return take_connection_locked();
  }

  size_t active_count() const {
    std::lock_guard lock(m_mtx);
    return active_connections;
  }

  size_t idle_count() const {
    std::lock_guard lock(m_mtx);
    return idle_connections;
  }

private:
  bool can_create_connection_locked() const noexcept {
    return idle_connections + active_connections < config.max_connections;
  }

  std::unique_ptr<db::core::Connection> take_connection_locked() {
    if (idle_connections > 0) {
      auto connection_ptr = std::move(connections.back());
      connections.pop_back();
      --idle_connections;
      ++active_connections;
      return connection_ptr;
    }
    if (init_new_connection_locked()) {
      auto connection_ptr = std::move(connections.back());
      connections.pop_back();
      --idle_connections;
      ++active_connections;
      return connection_ptr;
    }
    return nullptr;
  }

  bool init_new_connection_locked() {
    if (!can_create_connection_locked()) {
      return false;
    }
    auto connection = std::make_unique<db::core::Connection>(connection_config);
    connections.emplace_back(std::move(connection));
    ++idle_connections;
    return true;
  }

  void shutdown() {
    std::lock_guard lock(m_mtx);
    m_shutdown = true;
    m_cond.notify_all();
  }

private:
  mutable std::mutex m_mtx;
  std::condition_variable m_cond;
  PoolConfig config{};
  db::core::ConnectionConfig connection_config{};
  std::vector<std::unique_ptr<db::core::Connection>> connections{};
  size_t active_connections{0};
  size_t idle_connections{0};
  bool m_shutdown{false};
};
}; // namespace db::pool
