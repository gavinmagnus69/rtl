#include <iostream>


#include "ConnectionProvider.h"
#include "spdlog/spdlog.h"


rtl::ConnectionProvider::ConnectionProvider(std::shared_ptr<IProtocol> protoInstance)
    : m_protocolInstance(protoInstance) {
    checkInitialization();
}


rtl::ConnectionProvider::~ConnectionProvider() {
}


bool rtl::ConnectionProvider::IsInitialized() const {
    return m_statusFlag;
}


bool rtl::ConnectionProvider::checkInitialization() {
    m_statusFlag.exchange(true);
    if (m_protocolInstance == nullptr) {
        m_statusFlag.exchange(false);
    }
    if (!m_protocolInstance->isInitialized()) {
        m_statusFlag.exchange(false);
    }
    return m_statusFlag;
}


bool rtl::ConnectionProvider::SendData(std::shared_ptr<IProtocolMessage> msg) {
    if (!this->IsInitialized()) {
        return false;
    }
    if (!msg) {
        return false;
    }
    SPDLOG_INFO("Sending protocol message");
    m_protocolInstance->SendProtocolMessage(msg);
    SPDLOG_INFO("Sended protocol message");
    return true;
}

// thread-safe function
bool rtl::ConnectionProvider::ListenForData(std::shared_ptr<IProtocolMessage> msg, int32_t timeOutMs) {
    if (!this->IsInitialized()) {
        return false;
    }
    if (!msg) {
        return false;
    }
    std::lock_guard lock(m_mutex);
    return m_protocolInstance->ListenForMessage(msg, timeOutMs);
}
