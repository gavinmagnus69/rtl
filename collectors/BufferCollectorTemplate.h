#ifndef rtl_collectors_buffercollectortemplate_h
#define rtl_collectors_buffercollectortemplate_h


#include <list>
#include <mutex>

namespace rtl {
template <typename T>
class BufferCollectorTemplate {
public:
    BufferCollectorTemplate() {
        mMaxBufferSize = mBuffer.max_size();
    }
    ~BufferCollectorTemplate() = default;
public:
    bool takeData(T data) override {
        if (!data) {
            return false;
        }
        std::lock_guard lock(mBufferMutex);
        mBuffer.emplace_back(data);
        if (mBuffer.size() > mMaxBufferSize) {
            mBuffer.pop_front();
        }
        // SPDLOG_INFO(mBuffer.size());
        return true;
    }

    bool empty() const {
        std::lock_guard lock(mBufferMutex);
        return mBuffer.empty();
    }
    size_t size() const {
        std::lock_guard lock(mBufferMutex);
        return mBuffer.size();
    }
    void clear() {
        std::lock_guard lock(mBufferMutex);
        mBuffer.clear();
    }

    void setMaxSize(size_t newSize) {
        std::lock_guard lock(mBufferMutex);
        mMaxBufferSize = std::min(newSize, mBuffer.max_size());
        while (mBuffer.size() > mMaxBufferSize)
            mBuffer.pop_front();
    }

    T popFirstBlock() {
        T theResult;
        {
            std::lock_guard lock(mBufferMutex);
            if (!mBuffer.empty()) {
                theResult = mBuffer.front();
                mBuffer.pop_front();
            }
        }
        return theResult;
    }
    T popLastBlock() {
        T theResult;
        {
            std::lock_guard lock(mBufferMutex);
            if (!mBuffer.empty()) {
                theResult = mBuffer.back();
                mBuffer.pop_back();
            }
        }
        return theResult;
    }
    T getLastBlock() {
        T theResult;
        {
            std::lock_guard lock(mBufferMutex);
            if (!mBuffer.empty()) {
                theResult = mBuffer.back();
            }
        }
        return theResult;
    }

    std::list<T> profiles() const {
        std::lock_guard lock(mBufferMutex);
        return mBuffer;
    }
protected:
    size_t mMaxBufferSize;
private:
    mutable std::mutex mBufferMutex;
    std::list<T> mBuffer;
};
}; // namespace rtl


#endif