#ifndef rtl_serializer_h
#define rtl_serializer_h


#include <format>
#include <iostream>
#include <numbers>
#include <optional>
#include <string>
#include <vector>


#include "Request.h"


namespace rtl {
using namespace Request;

constexpr size_t messageSizes[20] = {sizeof(TestStruct), sizeof(MessageStruct)};

// old serialization
std::string serializeMessage(const BaseMessage& msg) {
    std::string binaryData;
    binaryData.append(reinterpret_cast<const char*>(msg.type()), 1);
    binaryData.append(reinterpret_cast<const char*>(&msg), msg.size());
    return binaryData;
}


std::optional<MessageType> deserializeMessage(const std::string& binaryData, std::string& mBuf) {
    if (binaryData.empty()) {
        return std::nullopt;
    }
    mBuf.clear();
    MessageType type = (MessageType)binaryData[0];
    mBuf.append(binaryData.substr(1, binaryData.size() - 1));
    return type;
}

}; // namespace rtl
#endif