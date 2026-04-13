#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Config.hpp"

#include "WsEngine.hpp"
#include "WsProtocol.hpp"
#include "WsRouter.hpp"
#include "WsSession.hpp"

#include "IExecutor.hpp"
#include "boost/asio/post.hpp"
#include "boost/beast/http/status.hpp"
#include "nlohmann/json_fwd.hpp"
#include "spdlog/spdlog.h"

namespace ws::server {

using JsonMessageHandler =
    std::function<void(WsSession &, const JsonMessage &)>;

class WsServer {
public:
  WsServer(core::config::Config &config,
           std::shared_ptr<rtl::stp::IExecutor> executor = nullptr)
      : m_coreConfig(config), m_executor(std::move(executor)),
        m_router(std::make_shared<WsRouter>()),
        m_engine(m_coreConfig, executor, m_router),
        m_idleTimer(m_engine.get_executor()) {
    setup_handlers_for_router();
  }

  ~WsServer() {
    // WsEngine destructor will be called here, so no need to call stop
  }

public:
  void on_message(WsRouter::MessageHandler handler) {
    if (!handler) {
      SPDLOG_ERROR("Handler nullptr");
      return;
    }
    m_usrOnMessage = std::move(handler);
  };

  void on_close(WsRouter::CloseHandler handler) {
    if (!handler) {
      SPDLOG_ERROR("Handler nullptr");
      return;
    }
    m_usrOnClose = handler;
  };

  void on_error(WsRouter::ErrorHandler handler) {
    if (!handler) {
      SPDLOG_ERROR("Handler nullptr");
      return;
    }
    m_usrOnError = handler;
  };

  void on_protocol_error(WsRouter::ProtocolErrorHandler handler) {
    if (!handler) {
      SPDLOG_ERROR("Handler nullptr");
      return;
    }
    m_usrOnProtoErr = std::move(handler);
  };

  void on_open(WsRouter::OpenHandler handler) {
    if (!handler) {
      SPDLOG_ERROR("Handler nullptr");
      return;
    }
    m_usrOnOpen = handler;
  };

  void on_accept(WsRouter::AcceptingHandler handler) {
    if (!handler) {
      SPDLOG_ERROR("Handler nullptr");
      return;
    }
    m_usrOnAcpt = handler;
  };

  void on_typed_message(JsonMessageHandler handler) {
    if (!handler) {
      SPDLOG_ERROR("Handler nullptr");
      return;
    }
    m_usrOnTyped = std::move(handler);
  };

  void on_auth(WsRouter::AuthHandler handler) {
    if (!handler) {
      SPDLOG_ERROR("Handler nullptr");
      return;
    }
    m_usrOnAuth = std::move(handler);
  };

  void start() {
    {
      std::lock_guard lock(m_sessionMtx);
      if (m_serverState == ServerState::running) {
        return;
      }
    }
    SPDLOG_INFO("Websocket server started on port: {}", port());
    m_engine.run();
    std::lock_guard lock(m_sessionMtx);
    m_serverState = ServerState::running;
  };

  void stop() {
    {
      std::lock_guard lock(m_sessionMtx);
      if (m_serverState == ServerState::stopping ||
          m_serverState == ServerState::stopped) {
        return;
      }
      m_serverState = ServerState::stopping;
    }
    m_engine.stop_accepting();
    stop_active_sessions();
    m_engine.stop_io();
    m_engine.join_io_threads();
    std::lock_guard lock(m_sessionMtx);
    m_serverState = ServerState::stopped;
  }

  void start_blocking() {
    start();
    for (; !m_engine.is_stop_requested();) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    };
  }

  uint16_t port() const {
    return static_cast<uint16_t>(m_coreConfig.getInt("websocket.port", 9090));
  };

  void broadcast_text(const std::string &text) {
    std::lock_guard lock(m_sessionMtx);
    if (!is_running()) {
      return;
    }
    remove_invalid_sessions_unlocked();
    for (auto &session : m_sessions) {
      if (auto ss = session.lock()) {
        ss->send_text(text);
      }
    }
  }

  void broadcast_binary(const void *data, size_t bytes) {
    std::lock_guard lock(m_sessionMtx);
    if (!is_running()) {
      return;
    }
    remove_invalid_sessions_unlocked();
    for (auto &session : m_sessions) {
      if (auto ss = session.lock()) {
        ss->send_binary(data, bytes);
      }
    }
  }

  void broadcast_json(const nlohmann::json &json) {
    broadcast_text(json.dump());
  }

  void broadcast_message(const JsonMessage &message) {
    broadcast_text(message_to_json(message).dump());
  }

private:
  void start_wait_timer() {
    m_stopWait.store(false, std::memory_order_relaxed);
    m_idleTimer.expires_after(
        std::chrono::seconds(m_engine.config().drainTimeout));
    m_idleTimer.async_wait([this](const erc &ec) { on_wait_timer_timeout(); });
  };

  void cancel_wait_timer() { m_idleTimer.cancel(); };

  void on_wait_timer_timeout() {
    cancel_wait_timer();
    m_stopWait.store(true, std::memory_order_relaxed);
    for (auto &session : get_live_sessions()) {
      session->close_forcefully();
    }
    m_cv.notify_all();
  };

  void wait_for_sessions_stop() {
    start_wait_timer();
    std::unique_lock lock(m_sessionMtx);
    m_cv.wait(lock, [this]() {
      remove_invalid_sessions_unlocked();
      return m_sessions.empty() || m_stopWait.load();
    });
    cancel_wait_timer();
  };

  void stop_active_sessions() {
    {
      std::lock_guard lock(m_sessionMtx);
      if (m_serverState == ServerState::stopped) {
        return;
      }
    }
    for (auto &session : get_live_sessions()) {
      session->close_for_shutdown();
    }
    wait_for_sessions_stop();
  };

  std::vector<std::shared_ptr<WsSession>> get_live_sessions() {
    std::lock_guard lock(m_sessionMtx);
    remove_invalid_sessions_unlocked();
    std::vector<std::shared_ptr<WsSession>> active_sessions;
    active_sessions.reserve(m_sessions.size());
    for (auto ptr : m_sessions) {
      if (auto sh_ptr = ptr.lock(); sh_ptr) {
        active_sessions.emplace_back(std::move(sh_ptr));
      }
    }
    return active_sessions;
  };

  void setup_handlers_for_router() {
    SPDLOG_INFO("Setuping base handlers");
    if (!m_router) {
      SPDLOG_ERROR("WsRouter is nullptr");
      return;
    }

    m_router->on_accept([this](WsSession &session) -> WsAcceptingDecision {
      if (m_usrOnAcpt) {
        return m_usrOnAcpt(session);
      }
      return WsAcceptingDecision::default_accept();
    });

    m_router->on_close([this](WsSession &session) {
      auto shared_session = session.shared_from_this();
      unregister_session(shared_session);
      if (m_usrOnClose) {
        m_usrOnClose(session);
      }
    });

    m_router->on_open([this](WsSession &session) {
      auto shared_session = session.shared_from_this();
      register_session(shared_session);
      if (m_usrOnOpen) {
        m_usrOnOpen(session);
      }
    });

    m_router->on_message(
        [this](WsSession &session, const std::string &payload) {
          if (m_usrOnMessage) {
            m_usrOnMessage(session, payload);
          }
          auto typed_message = string_to_message(payload);
          if (!typed_message.has_value()) {
            m_router->handle_protocol_error(session,
                                            "Failed to parse protocol message");
            return;
          }
          if (m_usrOnTyped) {
            m_usrOnTyped(session, *typed_message);
          }
        });

    m_router->on_error([this](WsSession &session, const erc &ec) {
      if (m_usrOnError) {
        m_usrOnError(session, ec);
      }
    });

    m_router->on_protocol_error(
        [this](WsSession &session, const std::string &msg) {
          if (m_usrOnProtoErr) {
            m_usrOnProtoErr(session, msg);
          }
        });

    m_router->on_auth([this](WsSession &session) {
      if (m_usrOnAuth) {
        return m_usrOnAuth(session);
      }
      return WsAcceptingDecision::default_accept();
    });
  }

  void register_session(std::shared_ptr<WsSession> session) {
    if (!session) {
      return;
    }
    std::lock_guard lock(m_sessionMtx);
    m_sessions.emplace_back(std::move(session));
  }

  void unregister_session(std::shared_ptr<WsSession> session) {
    if (!session) {
      return;
    }
    {
      std::lock_guard lock(m_sessionMtx);
      std::erase_if(
          m_sessions,
          [&session](const std::weak_ptr<WsSession> &weak_session) -> bool {
            auto sp = weak_session.lock();
            return !sp || sp.get() == session.get();
          });
    }
    m_cv.notify_all();
  };

  void remove_invalid_sessions_unlocked() {
    std::erase_if(m_sessions,
                  [](const std::weak_ptr<WsSession> &s_ptr) -> bool {
                    return s_ptr.expired();
                  });
  };

  bool is_running() const noexcept {
    return m_serverState == ServerState::running;
  };

private:
  enum class ServerState : uint8_t {
    init,
    running,
    stopping,
    stopped,
  };
  core::config::Config &m_coreConfig;
  std::shared_ptr<WsRouter> m_router{nullptr};
  WsEngine m_engine;
  std::vector<std::weak_ptr<WsSession>> m_sessions{};
  mutable std::mutex m_sessionMtx{};
  std::condition_variable m_cv;
  WsRouter::MessageHandler m_usrOnMessage;
  WsRouter::CloseHandler m_usrOnClose;
  WsRouter::ErrorHandler m_usrOnError;
  WsRouter::OpenHandler m_usrOnOpen;
  WsRouter::AcceptingHandler m_usrOnAcpt;
  WsRouter::ProtocolErrorHandler m_usrOnProtoErr;
  WsRouter::AuthHandler m_usrOnAuth;
  JsonMessageHandler m_usrOnTyped;
  boost::asio::steady_timer m_idleTimer; // 120 bytes
  std::atomic_bool m_stopWait{false};
  ServerState m_serverState{ServerState::init};
  std::shared_ptr<rtl::stp::IExecutor> m_executor{nullptr};
};

}; // namespace ws::server