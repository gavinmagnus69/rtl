#pragma once

#include <functional>
#include <thread>
#include <vector>


namespace rtl {
namespace stp {

using Task = std::function<void()>;
class ThreadPool {
private:
    std::vector<std::thread> m_activeWorkers;
    size_t m_maxThreadCount{0};
    size_t m_currentThreadCount{0};
};

}; // namespace stp


}; // namespace rtl