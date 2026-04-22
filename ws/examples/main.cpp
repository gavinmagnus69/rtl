#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "App.hpp"

#include "Config.hpp"
#include "Logger.hpp"
#include "WsApp.hpp"
#include "WsEngine.hpp"
#include "WsProtocol.hpp"
#include "WsRouter.hpp"
#include "WsServer.hpp"
#include "WsSession.hpp"
#include "nlohmann/detail/conversions/to_json.hpp"
#include "spdlog/spdlog.h"

namespace {

const std::filesystem::path g_defaultConfigPath{
    R"(C:\personal\course_work\src\libs\ws\examples\config\config.json)"};

const std::filesystem::path g_pathToConfigMacos{
    "/Users/romanahmetov/Desktop/personal/projects/course_work/src/libs/ws/"
    "examples/config/config.json"};
const std::filesystem::path g_pathToConfigWin{};

std::atomic_bool g_stopRequested{false};

void on_signal(int) { g_stopRequested.store(true, std::memory_order_relaxed); }

std::filesystem::path resolve_config_path(int argc, char **argv) {
  if (argc > 1 && argv[1] && std::string{argv[1]}.size() > 0) {
    return std::filesystem::path{argv[1]};
  }
  return g_defaultConfigPath;
}

std::string messagejson_to_string(const ws::server::JsonMessage &message) {
  return ws::server::message_to_json(message).dump();
}

} // namespace

namespace wss = ws::server;

int run_wsApp(int argc, char **argv) {
  using EndpointHandler =
      std::function<void(wss::WsSession &, const wss::JsonMessage &)>;
  try {
    SPDLOG_INFO("run_wsServer called");
    const auto configPath = resolve_config_path(argc, argv);
    SPDLOG_INFO("Config path {}", configPath.string());
    application::app::App app{configPath.string()};
    app.run();
    while (!g_stopRequested.load(std::memory_order_relaxed)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    app.stop();
    return 0;

  } catch (const std::exception &exp) {
    SPDLOG_ERROR(exp.what());
    return -1;
  }

  return 0;
}

void test_message_to_json_dump() {
  wss::JsonMessage defaultMsg;
  SPDLOG_INFO(messagejson_to_string(defaultMsg));
}
int main(int argc, char **argv) {
  std::signal(SIGINT, on_signal);
#ifdef SIGTERM
  std::signal(SIGTERM, on_signal);
#endif
  core::logger::Logger logger;
  test_message_to_json_dump();
  //   test_message_to_json_dump();
  return run_wsApp(argc, argv);
}
