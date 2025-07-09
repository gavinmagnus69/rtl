#include <iostream>


#include "I7580Protocol.h"
#include "spdlog/spdlog.h"

// constexpr uint32_t maxBufferSize = 376;

// creates channel inside and sets flag of success
rtl::I7580Protocol::I7580Protocol(uint16_t comPort, uint32_t baudrate) {
    if (!initChannel(comPort, baudrate)) {
        m_initStatus = false;
        return;
    }
    m_initStatus = true;
    m_buffer.reserve(CAMSO_MAX_PACKET_SIZE);
}


rtl::I7580Protocol::~I7580Protocol() {
}

// means that it gets I7580Message (that implements logic of retreving Request::MessageStruct)
bool rtl::I7580Protocol::SendProtocolMessage(std::shared_ptr<IProtocolMessage> msg) {
    if (!isInitialized()) {
        return false;
    }
    // TODO: do not send data until it will be received
    if (!m_channel->Send_Data((uint8_t*)msg->data(), msg->size())) {
        SPDLOG_WARN("Data not sent");
        return false;
    }
    return true;
}


bool rtl::I7580Protocol::ListenForMessage(std::shared_ptr<IProtocolMessage> msg, int32_t timeOutMs = 0) {
    if (!isInitialized()) {
        return false;
    }
    if (msg == nullptr) {
        return false;
    }
    // SPDLOG_INFO("Start reading");
    uint32_t inputDataSize = 0;
    if (!m_channel->Get_Data(reinterpret_cast<uint8_t*>(m_buffer.data()), CAMSO_MAX_PACKET_SIZE, inputDataSize, 0, timeOutMs)) {
        // SPDLOG_WARN("No input data");
        return false;
    }
    // std::cout << "received data\n";
    // for(int i = 0; i < inputDataSize; ++i) {
    //     std::cout << (uint16_t)m_buffer.data()[i] << ' ';
    // }
    msg->setData(m_buffer.c_str(), inputDataSize);
    return true;
}


bool rtl::I7580Protocol::initChannel(uint16_t comPort, uint32_t baudrate) {
    m_channel = std::make_unique<I7580::CamsoChannel>();
    std::string initString = "\\\\.\\COM" + std::to_string(comPort) + ";" + std::to_string(baudrate);
    if (!m_channel->Init(initString)) {
        SPDLOG_ERROR("Output channel not init");
        return false;
    }
    SPDLOG_INFO("I7580Protocol initialized com_port = {} baudrate = {}", comPort, baudrate);
    return true;
}

// returns status flag of initialization
bool rtl::I7580Protocol::isInitialized() const {
    return m_initStatus;
}
