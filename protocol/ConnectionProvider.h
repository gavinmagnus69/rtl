#ifndef rtl_connection_provider_h
#define rtl_connection_provider_h


#include <atomic>
#include <memory>
#include <mutex>


#include "IProtocol.h"
#include "IProtocolMessage.h"


namespace rtl {
class ConnectionProvider {
public:
    // set protocol to use that implements sending and receiving
    ConnectionProvider(std::shared_ptr<IProtocol>);
    ConnectionProvider() = delete;
    ConnectionProvider(const ConnectionProvider&) = delete;
    ConnectionProvider(ConnectionProvider&&) = delete;
    ConnectionProvider& operator=(const ConnectionProvider&) = delete;
    ~ConnectionProvider();
public:
    // checks health of components
    bool IsInitialized() const;
    bool SendData(std::shared_ptr<IProtocolMessage>);
    bool ListenForData(std::shared_ptr<IProtocolMessage>, int32_t timeOutMs);
private:
    bool checkInitialization();
    std::mutex m_mutex;
    std::atomic<bool> m_statusFlag = false;
    std::shared_ptr<IProtocol> m_protocolInstance;
};

}; // namespace rtl

#endif