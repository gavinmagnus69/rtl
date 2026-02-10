#include "ThreadPoolExecutor.hpp"


namespace rtl {
namespace stp {

auto makeThreadPoolExecutor(size_t current_threads, size_t max_threads) -> std::unique_ptr<IExecutor> {
    return std::make_unique<ThreadPoolExecutor>(current_threads, max_threads);
}

}
} // namespace rtl
