#include "WsClient.hpp"
#include "WsProtocol.hpp"
#include "spdlog/spdlog.h"

int main() {
    auto client = ws::server::WsClient::create("localhost", "9090", "/");
    client->on_open([] { std::cout << "Connected to server\\n"; });
    client->on_message([](const std::string& msg) { std::cout << "Server says: " << msg << "\\n"; });
    client->enable_auto_reconnect(true, std::chrono::seconds(3));
    client->enable_heartbeat(std::chrono::seconds(30));
    SPDLOG_INFO("Connecting to server");
    client->connect();
    for (std::string line; std::getline(std::cin, line);) {
        SPDLOG_INFO(line);
        if (line == "/quit") {
            SPDLOG_INFO("quit requested");
            break;
        }
        ws::server::JsonMessage msg;
        msg.sender = "client228";
        msg.error = line;
        client->send_json_message("msg.echo", msg);
        // client->send_text("hello");
    }
    client->close();
};