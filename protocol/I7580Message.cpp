#include <iostream>


#include "I7580Message.h"
#include "spdlog/spdlog.h"


rtl::I7580Message::I7580Message() {
    reserveBufferSize(bufSize);
}

//gets specialized struct (MessageStruct)
rtl::I7580Message::I7580Message(const Request::IBaseMessage& msg) {
    reserveBufferSize(bufSize);
    setData(msg.data(), msg.size());
}


rtl::I7580Message::I7580Message(std::shared_ptr<Request::IBaseMessage> msg) {
    reserveBufferSize(bufSize);
    setData(msg);
}


//const char* of m_buffer
const char* rtl::I7580Message::data() {
    return m_buffer.c_str();
}

//reserve bytes of m_buffer
void rtl::I7580Message::reserveBufferSize(uint64_t bufSize) {
    if(m_buffer.capacity() > bufSize) {
        return;
    }
    m_buffer.reserve(bufSize);
}

//returns size of m_data
size_t rtl::I7580Message::size() const {
    return m_buffer.size();
}

// takes only meaning bytes in structure (means: not including aligning)
void rtl::I7580Message::setData(const Request::MessageStruct& str) {
    m_buffer.clear();
    // what stores original structure
    // for(int i = 0; i < str.size(); ++i) {
    //     std::cout << (uint16_t)str.data()[i] << ' ';
    // }
    // std::cout << "\nabove is original dump of structure, below is m_buffer\n";
    m_buffer.append(str.data(), 7);
    m_buffer.append(str.data() + 8, 8);
    // m_buffer.append(str.data(), str.size());
    // debug
    // for(int i = 0; i < m_buffer.size(); ++i) {
    //     std::cout << (uint16_t)m_buffer[i] << ' ';
    // }
    // std::cout << "\ndebugend\n";
}

//dddddddadddddddd -> structure (where: d - meaningful byte, a - aligned byte)
void rtl::I7580Message::setData(const char* dataBuf, size_t dataSize) {
    m_buffer.clear();
    m_buffer.append(dataBuf, dataSize);
}


void rtl::I7580Message::setData(std::shared_ptr<Request::IBaseMessage> msg) {
    if(msg == nullptr) {
        return;
    }
    m_buffer.clear();
    auto alignedBytes = msg->alignedBytes();
    for(int i = 0; i < msg->size(); ++i) {
        if(alignedBytes.contains(i)){
            continue;
        }
        m_buffer.push_back(msg->data()[i]);
    }
}

//returns char* of m_buffer
char* rtl::I7580Message::raw() {
    return m_buffer.data();
}

//returns Request::MessageStruct builded of meaningful bytes of m_buffer
std::optional<rtl::Request::MessageStruct> rtl::I7580Message::getMessageStruct() const {
    Request::MessageStruct str;
    auto alignedBytes = str.alignedBytes();
    if(m_buffer.empty()) {
        return std::nullopt;
    }
    if(m_buffer.size() < sizeof(Request::MessageStruct) - alignedBytes.size()) {
        return std::nullopt;
    }
    memcpy((void*)str.data(), m_buffer.data(), 7);
    memcpy((void*)str.data(), m_buffer.data() + 8, 8);
    return str;
}

// returns any message derived from IBaseMessage
bool  rtl::I7580Message::getStruct(std::shared_ptr<Request::IBaseMessage> msg) {
    if(msg == nullptr) {
        return false;
    }
    auto alignedBytes = msg->alignedBytes();
    if(m_buffer.empty()) {
        return false;
    }
    if(m_buffer.size() < sizeof(msg->size()) - alignedBytes.size()) {
        return false;
    }
    uint32_t offset = 0;
    for(int i = 0; i < msg->size(); ++i) {
        if(alignedBytes.contains(i)) {
            continue;
        }
        msg->raw()[i] = m_buffer[offset];
        ++offset;
    }
    return true;
}


void rtl::I7580Message::clear() {
    m_buffer.clear();
}




