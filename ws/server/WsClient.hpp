#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <cstddef>
#include <exception>
#include <string_view>
#include "boost/asio/connect.hpp"
#include "boost/asio/error.hpp"
#include "boost/asio/executor_work_guard.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/post.hpp"
#include "boost/beast/core/buffers_to_string.hpp"
#include "boost/beast/core/role.hpp"
#include "boost/beast/websocket/error.hpp"
#include "boost/beast/websocket/impl/error.hpp"
#include "boost/beast/websocket/rfc6455.hpp"
#include "boost/beast/websocket/stream.hpp"
#include "boost/beast/websocket/stream_base.hpp"
#include "boost/system/detail/error_code.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "Logger.hpp"
#include "WsProtocol.hpp"

#include "spdlog/spdlog.h"

namespace ws::server {

namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;
using erc = boost::system::error_code;

class WsClient : public std::enable_shared_from_this<WsClient> {
public:
    using OpenHandler = std::function<void()>;
    using MessageHandler = std::function<void(const std::string&)>;
    using CloseHandler = std::function<void()>;
    using ErrorHandler = std::function<void(const erc&)>;
public:
    static std::shared_ptr<WsClient> create(std::string_view host, std::string_view port, std::string_view target) {
        return std::shared_ptr<WsClient>(new WsClient(host, port, target));
    };

    void on_open(OpenHandler handler) {
        m_onOpen = std::move(handler);
    }
    void on_close(CloseHandler handler) {
        m_onClose = std::move(handler);
    };
    void on_message(MessageHandler handler) {
        m_onMessage = std::move(handler);
    };
    void on_error(ErrorHandler handler) {
        m_onError = std::move(handler);
    };

    void enable_auto_reconnect(bool enable, std::chrono::seconds delay = std::chrono::seconds{3}) {
        m_autoReconnect.store(enable);
        m_reconnectDelay = delay;
    };

    void enable_heartbeat(std::chrono::seconds interval) {
        if (interval.count() <= 0) {
            m_heartbeatEnabled.store(false);
            return;
        }
        m_heartbeatEnabled.store(true);
        m_heartbeatInterval = interval;
    };

    ~WsClient() {
        try {
            close();
        } catch (const std::exception& exp) {
            SPDLOG_ERROR(exp.what());
        }
    };

    void connect() {
        if (!m_alive.load(std::memory_order_relaxed) || m_closing.load(std::memory_order_relaxed)) {
            return;
        };
        bool expected = false;
        if (!m_started.compare_exchange_strong(expected, true)) {
            return;
        }
        init_io();
        auto self = shared_from_this();
        m_ioThread = std::thread([self]() {
            try {
                self->m_ioc->run();
            } catch (const std::exception& exp) {
                erc ec;
                self->emit_error(ec, exp.what());
            }
        });
        net::post(m_ws->get_executor(), [self]() { self->do_resolve(); });
    };

    void close() {
        if (m_closing.exchange(true)) {
            return;
        }
        m_alive.store(false, std::memory_order_relaxed);
        m_connected.store(false, std::memory_order_relaxed);
        m_heartbeatStop.store(true, std::memory_order_relaxed);
        m_autoReconnect.store(false, std::memory_order_relaxed);
        m_reconnectScheduled.store(false, std::memory_order_relaxed);
        auto self = shared_from_this();
        if (m_ws) {
            net::post(m_ws->get_executor(), [self]() {
                self->m_ws->async_close(websocket::close_code::normal, [self](const erc& ec2) {
                    if (ec2 && ec2 != net::error::operation_aborted) {
                        self->emit_error(ec2, "close");
                    }
                });
            });
        }
        m_workGuard.reset();
        if (m_ioc) {
            m_ioc->stop();
        }
        if (m_ioThread.joinable() && std::this_thread::get_id() != m_ioThread.get_id()) {
            m_ioThread.join();
        }
        if (m_heartbeatThread.joinable() && std::this_thread::get_id() != m_heartbeatThread.get_id()) {
            m_heartbeatThread.join();
        }
    };

    void send_ping() {
        if (!m_connected || m_closing || !m_ws) {
            return;
        }
        auto self = shared_from_this();
        net::post(m_ws->get_executor(), [self]() {
            if (!self->m_ws || self->m_closing) {
                return;
            }
            websocket::ping_data data;
            self->m_ws->async_ping(data, [self](const erc& ec) {
                if (ec && ec != net::error::operation_aborted) {
                    self->emit_error(ec, "ping");
                    self->maybe_schedule_reconnect(ec);
                }
            });
        });
    };

    void do_write() {
        if (!m_ws || m_closing) {
            return;
        }
        {
            std::lock_guard lock(m_writeMutex);
            if (m_writeQueue.empty()) {
                m_writeInProgress = false;
                return;
            }
        }
        auto self = shared_from_this();
        m_ws->async_write(net::buffer(self->m_writeQueue.front()), [self](const erc& ec, size_t bytes) {
            {
                std::lock_guard lock(self->m_writeMutex);
                if (!self->m_writeQueue.empty()) {
                    self->m_writeQueue.pop_front();
                }
                if (ec || self->m_writeQueue.empty()) {
                    self->m_writeInProgress = false;
                }
            }
            if (ec && ec != net::error::operation_aborted) {
                self->emit_error(ec, "do_write");
                self->maybe_schedule_reconnect(ec);
                return;
            }
            SPDLOG_INFO("Wrote {} bytes", bytes);
            self->do_write();
        });
    };

    void send_text(const std::string& text) {
        if (!m_connected || m_closing || !m_ws) {
            return;
        }
        auto self = shared_from_this();
        net::post(m_ws->get_executor(), [self, text]() {
            if (self->m_closing) {
                return;
            }
            bool shouldStartWrite = false;
            {
                std::lock_guard lock(self->m_writeMutex);
                self->m_writeQueue.push_back(text);
                if (!self->m_writeInProgress) {
                    self->m_writeInProgress = true;
                    shouldStartWrite = true;
                }
            }
            if (shouldStartWrite) {
                self->do_write();
            }
        });
    };

    void send_json_message(const std::string& type, const nlohmann::json& payload) {
        JsonMessage msg;
        msg.type = type;
        set_payload(msg, payload);
        auto jsonOpt = message_to_json(msg);
        send_text(jsonOpt.dump());
    };

    void send_json_message(const std::string& type, const JsonMessage& msg) {
        JsonMessage message = msg;
        message.type = type;
        send_text(message_to_json(message).dump());
    };
private:
    WsClient(std::string_view host, std::string_view port, std::string_view target)
        : m_host(host)
        , m_port(port)
        , m_target(target) {

        };

    void init_io() {
        if (m_ioc) {
            m_ioc->stop();
        }
        if (m_ioThread.joinable() && std::this_thread::get_id() != m_ioThread.get_id()) {
            m_ioThread.join();
        }
        if (m_heartbeatThread.joinable() && std::this_thread::get_id() != m_heartbeatThread.get_id()) {
            m_heartbeatThread.join();
        }
        m_ioc = std::make_unique<net::io_context>();
        m_workGuard = std::make_unique<net::executor_work_guard<net::io_context::executor_type>>(m_ioc->get_executor());
        m_resolver = std::make_unique<tcp::resolver>(*m_ioc);
        m_ws = std::make_unique<websocket::stream<tcp::socket>>(*m_ioc);
        m_buffer.clear();
        m_connected.store(false, std::memory_order_relaxed);
        m_closing.store(false, std::memory_order_relaxed);
        m_heartbeatStop.store(false, std::memory_order_relaxed);
        {
            std::lock_guard lock(m_writeMutex);
            m_writeQueue.clear();
            m_writeInProgress = false;
        }
    };

    void do_resolve() {
        auto self = shared_from_this();
        m_resolver->async_resolve(m_host, m_port, [self](const erc& ec, tcp::resolver::results_type res_typ) {
            if (ec) {
                self->emit_error(ec, "do_resolve");
                self->maybe_schedule_reconnect(ec);
                return;
            };
            SPDLOG_INFO("Resolved, trying to connect");
            self->do_connect(res_typ);
        });
    };

    void do_connect(const tcp::resolver::results_type& type) {
        auto self = shared_from_this();
        net::async_connect(m_ws->next_layer(), type, [self](const erc& ec, const tcp::endpoint&) {
            if (ec) {
                SPDLOG_ERROR(ec.message());
                self->emit_error(ec, "do_connect");
                self->maybe_schedule_reconnect(ec);
                return;
            }
            SPDLOG_INFO("Connected, trying to handshake");
            self->do_handshake();
        });
    };

    void do_handshake() {
        auto self = shared_from_this();
        std::string host_header = m_host + ":" + m_port;
        m_ws->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
        m_ws->async_handshake(host_header, m_target, [self](const erc& ec) {
            if (ec) {
                SPDLOG_ERROR(ec.message());
                self->emit_error(ec, "do_handshake");
                self->maybe_schedule_reconnect(ec);
                return;
            }
            self->m_connected.store(true, std::memory_order_relaxed);
            if (self->m_onOpen) {
                self->m_onOpen();
            }
            if (self->m_heartbeatEnabled) {
                self->start_heartbeat();
            }
            SPDLOG_INFO("Accepted, trying to read");
            self->do_read();
        });
    };

    void do_read() {
        auto self = shared_from_this();
        m_ws->async_read(m_buffer, [self](const erc& ec, size_t size) {
            if (ec) {
                SPDLOG_ERROR(ec.message());
                if (ec != websocket::error::closed && ec != net::error::operation_aborted) {
                    self->emit_error(ec, "read");
                }
                self->m_connected.store(false, std::memory_order_relaxed);
                if (self->m_onClose) {
                    self->m_onClose();
                }
                self->maybe_schedule_reconnect(ec);
                return;
            }
            auto data = beast::buffers_to_string(self->m_buffer.data());
            self->m_buffer.clear();
            if (self->m_onMessage) {
                self->m_onMessage(data);
            }
            if (!self->m_closing.load()) {
                self->do_read();
            }
        });
    };

    void start_heartbeat() {
        if (m_heartbeatThread.joinable()) {
            return;
        }
        auto self = shared_from_this();
        m_heartbeatThread = std::thread([self]() {
            while (!self->m_heartbeatStop.load(std::memory_order_relaxed) && self->m_alive.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(self->m_heartbeatInterval);
                if (!self->m_connected.load(std::memory_order_relaxed) || self->m_closing.load(std::memory_order_relaxed) || self->m_heartbeatStop.load(std::memory_order_relaxed)) {
                    continue;
                }
                self->send_ping();
            }
        });
    };

    void maybe_schedule_reconnect(const erc& ec) {
        if (!m_autoReconnect.load(std::memory_order_relaxed) || m_closing.load(std::memory_order_relaxed) || !m_alive.load(std::memory_order_relaxed)) {
            return;
        }
        if (ec == websocket::error::closed || ec == net::error::operation_aborted) {
            return;
        }
        bool expected = false;
        if (!m_reconnectScheduled.compare_exchange_strong(expected, true)) {
            return;
        }
        auto self = shared_from_this();
        std::thread([self]() {
            std::this_thread::sleep_for(self->m_reconnectDelay);
            if (!self->m_alive.load(std::memory_order_relaxed) || self->m_closing.load(std::memory_order_relaxed)) {
                self->m_reconnectScheduled.store(false, std::memory_order_relaxed);
                return;
            }
            self->m_started.store(false, std::memory_order_relaxed);
            self->m_reconnectScheduled.store(false, std::memory_order_relaxed);
            self->connect();
        }).detach();
    };

    void emit_error(const erc& ec, const char* msg) {
        if (m_onError) {
            m_onError(ec);
            return;
        }
        SPDLOG_ERROR("[Client][{}] error: {}", msg, ec.message());
    };
private:
    std::string m_host{};
    std::string m_port{};
    std::string m_target{};
    std::unique_ptr<net::io_context> m_ioc{nullptr};
    std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>> m_workGuard{nullptr};
    std::unique_ptr<tcp::resolver> m_resolver{nullptr};
    std::unique_ptr<websocket::stream<tcp::socket>> m_ws{nullptr};
    beast::flat_buffer m_buffer{};
    std::thread m_ioThread{};
    std::thread m_heartbeatThread{};
    std::atomic<bool> m_started{false};
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_closing{false};
    std::atomic<bool> m_alive{true};
    std::atomic<bool> m_heartbeatEnabled{false};
    std::atomic<bool> m_heartbeatStop{false};
    std::chrono::seconds m_heartbeatInterval{30};
    std::atomic<bool> m_autoReconnect{false};
    std::chrono::seconds m_reconnectDelay{3};
    std::atomic<bool> m_reconnectScheduled{false};
    OpenHandler m_onOpen;
    MessageHandler m_onMessage;
    CloseHandler m_onClose;
    ErrorHandler m_onError;
    std::mutex m_writeMutex;
    std::deque<std::string> m_writeQueue;
    bool m_writeInProgress{false};
};

}; // namespace ws::server
