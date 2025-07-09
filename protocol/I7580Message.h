#ifndef rtl_i7580message_h
#define rtl_i7580message_h


#include <optional>
#include <string>


#include "IProtocolMessage.h"
#include "Request.h"


namespace rtl {


constexpr uint32_t bufSize = 1024;


// this class on input gets Request::MessageStruct and converts it to raw bytes without aligning
// on output its using getMessageStruct method to return std::optional of stored data (already aligned structure)
class I7580Message final : public IProtocolMessage {
public:
    I7580Message();
    I7580Message(const Request::IBaseMessage&);
    I7580Message(std::shared_ptr<Request::IBaseMessage>);
public:
    void setData(const Request::MessageStruct&);
    void setData(const char*, size_t) override;
    void setData(std::shared_ptr<Request::IBaseMessage>) override;
    const char* data() override;
    size_t size() const override;
    char* raw() override;
    std::optional<Request::MessageStruct> getMessageStruct() const;
    bool getStruct(std::shared_ptr<Request::IBaseMessage>);
    void clear() override;
private:
    void reserveBufferSize(uint64_t);
    std::string m_buffer;
};
}; // namespace rtl

#endif