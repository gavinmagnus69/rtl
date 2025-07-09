#ifndef rtl_protocol_testtemplatemessagecontainer
#define rtl_protocol_testtemplatemessagecontainer


namespace rtl {
template <class T>
class TestMessageContainer {
public:
    const char* data() const {
        return reinterpret_cast<const char*>(&m_msg);
    }
    size_t size() const {
        return sizeof(m_msg);
    }
private:
    T m_msg;
};

}; // namespace rtl


#endif