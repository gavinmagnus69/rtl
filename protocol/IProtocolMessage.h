#ifndef rtl_iprotocolmessage_h
#define rtl_iprotocolmessage_h


#include <memory>


#include "Request.h"


namespace rtl {
class IProtocolMessage {
public:
    // virtual void setMessage(const Request::BaseMessage&, Request::MessageType) = 0;
    virtual const char* data() = 0;
    virtual char* raw() = 0;
    virtual void setData(const char*, size_t) = 0;
    virtual void setData(std::shared_ptr<Request::IBaseMessage>) = 0;
    virtual size_t size() const = 0;
    virtual void clear() = 0;
    virtual ~IProtocolMessage() = default;
};


}; // namespace rtl

#endif