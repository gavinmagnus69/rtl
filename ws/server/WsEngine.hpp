#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>

#include "WsConfig.hpp"
#include "WsRouter.hpp"
#include "WsSession.hpp"

#include "Config.hpp"
#include "Logger.hpp"

#include "IExecutor.hpp"
#include "boost/asio/io_context.hpp"

namespace ws::server {

static const rtl::stp::TaskOptions g_nonPerTaskOptions{
    .is_periodic = false, .periodic_interval_ms = 0};

namespace net = boost::asio;
namespace beast = boost::beast;
using tcp = net::ip::tcp;
using erc = boost::system::error_code;

void set_affinity_for_current_thread(size_t threadIndex) {
#ifdef __linux__
  unsigned int hc = std::thread::hardware_concurrency();
  if (hc == 0u)
    hc = 1u;

  const unsigned int cpu =
      static_cast<unsigned int>(threadIndex % static_cast<std::size_t>(hc));

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);

  (void)pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#else
  (void)threadIndex;
#endif
}

class WsEngine {
public:
  WsEngine(core::config::Config &config,
           std::shared_ptr<rtl::stp::IExecutor> executor,
           std::shared_ptr<WsRouter> router)
      : m_coreConfig(config), m_executor(std::move(executor)),
        m_router(std::move(router)),
        m_ioContext(std::make_shared<boost::asio::io_context>()) {
    initEngine();
  };

  ~WsEngine() {
    stop();
    SPDLOG_INFO("WsEngine destructor called.");
  };

  void run() {
    m_stopRequested.store(false, std::memory_order_relaxed);
    start_accepting();
    start_io_threads();
  };

  void stop_async() {
    stop_accepting();
    stop_io();
  };

  void stop() {
    stop_async();
    join_io_threads();
  };

  bool is_stop_requested() const { return m_stopRequested.load(); };

  void join_io_threads() {
    for (auto &thread : m_ioThreads) {
      if (thread.joinable()) {
        thread.join();
      }
    }
    m_ioThreads.clear();
  }

  void stop_accepting() {
    m_stopRequested.store(true, std::memory_order_relaxed);
    if (m_acceptor && m_acceptor->is_open()) {
      erc ec;
      m_acceptor->close(ec);
      if (ec) {
        SPDLOG_ERROR("Error closing acceptor: {}", ec.message());
      }
    }
  };

  void stop_io() {
    if (m_ioContext) {
      m_ioContext->stop(); // signals to stop all threads running the io_context
    }
  };

  auto get_executor() { return m_ioContext->get_executor(); };

  WsConfig &config() { return m_config; };

private:
  void initEngine() {
    SPDLOG_INFO("WsEngine initEngine called.");
    int port = m_coreConfig.getInt("websocket.port", 8080);
    if (port <= 0 || port > 65535) {
      SPDLOG_ERROR(
          "Invalid WebSocket port number: {}. Using default port 8080.", port);
      port = 8080;
    }
    init_acceptor(static_cast<uint16_t>(port));
  };

  void init_acceptor(uint16_t port) {
    m_acceptor = std::make_unique<tcp::acceptor>(m_ioContext->get_executor());
    erc ec;
    erc all_ec;
    tcp::endpoint endpoint{tcp::v4(), port};
    all_ec = m_acceptor->open(endpoint.protocol(), ec);
    if (ec) {
      throw std::runtime_error("Failed to open acceptor: " + ec.message());
    }
    all_ec = m_acceptor->set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
      throw std::runtime_error("Failed to set reuse_address option: " +
                               ec.message());
    }
    all_ec = m_acceptor->bind(endpoint, ec);
    if (ec) {
      throw std::runtime_error("Failed to bind acceptor: " + ec.message());
    }
    all_ec = m_acceptor->listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
      throw std::runtime_error("Failed to listen on acceptor: " + ec.message());
    }
  };

  void start_accepting() {
    auto new_socket =
        std::make_shared<tcp::socket>(m_ioContext->get_executor());
    m_acceptor->async_accept(*new_socket, [this, new_socket](erc ec) {
      if (m_stopRequested.load(std::memory_order_relaxed)) {
        return;
      }
      if (ec) {
        SPDLOG_ERROR("Failed to accept new connection: {}", ec.message());
      } else if (!m_stopRequested.load(std::memory_order_relaxed)) {
        SPDLOG_INFO("New WebSocket connection accepted");
        handler_client(std::move(*new_socket));
      }
      if (!m_stopRequested.load(std::memory_order_relaxed)) {
        start_accepting();
      }
    });
  };

  void run_io_threads_on_executor(size_t threadCount) {
    if (m_executor) {
      for (size_t i = 0; i < threadCount; ++i) {
        m_executor->submit(g_nonPerTaskOptions, [this, threadIndex = i]() {
          try {
            set_affinity_for_current_thread(threadIndex);
            m_ioContext->run();
          } catch (const std::exception &ex) {
            SPDLOG_ERROR("Exception in I/O thread: {}", ex.what());
          } catch (...) {
            SPDLOG_ERROR("Unknown exception in I/O thread");
          }
          SPDLOG_INFO("I/O thread {} exiting", threadIndex);
        });
      }
      return;
    }
  }

  void run_io_threads_on_default_threads(size_t threadCount) {
    m_ioThreads.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
      m_ioThreads.emplace_back([this, threadIndex = i]() {
        try {
          set_affinity_for_current_thread(threadIndex);
          m_ioContext->run();
        } catch (const std::exception &ex) {
          SPDLOG_ERROR("Exception in I/O thread: {}", ex.what());
        } catch (...) {
          SPDLOG_ERROR("Unknown exception in I/O thread");
        }
        SPDLOG_INFO("I/O thread {} exiting", threadIndex);
      });
    }
  }

  void start_io_threads() {
    const size_t threadCount = compute_io_thread_count();
    SPDLOG_INFO("Starting {} I/O threads for WebSocket server", threadCount);
    if (m_executor) {
      run_io_threads_on_executor(threadCount);
      return;
    }
    run_io_threads_on_default_threads(threadCount);
  };

  void handler_client(tcp::socket socket) {
    auto new_session = std::make_shared<WsSession>(std::move(socket), m_config,
                                                   m_router, m_executor);
    new_session->run();
  };

  std::size_t compute_io_thread_count() const {
    const unsigned int hc = std::thread::hardware_concurrency();
    const unsigned int v = (hc != 0u) ? (hc / 2u) : 1u;
    return static_cast<std::size_t>(std::max(1u, v));
  };

private:
  core::config::Config &m_coreConfig;
  WsConfig m_config{};
  std::shared_ptr<WsRouter> m_router{nullptr};
  std::shared_ptr<boost::asio::io_context> m_ioContext{nullptr};
  std::unique_ptr<tcp::acceptor> m_acceptor{nullptr};
  std::vector<std::thread> m_ioThreads;
  std::atomic<bool> m_stopRequested{false};
  std::shared_ptr<rtl::stp::IExecutor> m_executor{nullptr};
};
}; // namespace ws::server
