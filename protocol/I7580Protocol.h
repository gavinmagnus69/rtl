#ifndef protocol_protocol_h
#define protocol_protocol_h


#include <memory>


#include <CamsoChannel.h>
#include "IProtocol.h"


namespace rtl {
class I7580Protocol : public IProtocol {
public:
    I7580Protocol(uint16_t comPort, uint32_t baudrate);
    ~I7580Protocol() override;
public:
    bool SendProtocolMessage(std::shared_ptr<IProtocolMessage>) override;
    bool ListenForMessage(std::shared_ptr<IProtocolMessage>, int32_t timeOutMs) override;
    bool isInitialized() const override;
private:
    bool initChannel(uint16_t comPort, uint32_t baudrate);
    std::string m_buffer;
    bool m_initStatus = false;
    std::unique_ptr<I7580::Channel> m_channel;
};
} // namespace rtl

#endif