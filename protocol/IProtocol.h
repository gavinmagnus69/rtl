#ifndef protocol_iprotocol_h
#define protocol_iprotocol_h


#include <memory>
#include <string>


#include "IProtocolMessage.h"
// TODO: string uses heap, maybe we should use stack-allocated structures.
//  Possible implementation of I-7580 protocol

namespace rtl {
class IProtocol {
public:
    virtual bool SendProtocolMessage(std::shared_ptr<IProtocolMessage>) = 0;
    virtual bool ListenForMessage(std::shared_ptr<IProtocolMessage>, int32_t timeOutMs) = 0;
    virtual bool isInitialized() const = 0;
    virtual ~IProtocol() = default;
};
}; // namespace rtl


#endif