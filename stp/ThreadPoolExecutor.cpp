#include "ThreadPoolExecutor.hpp"


namespace rtl {
namespace stp {

auto makeThreadPoolExecutor(size_t current_threads, size_t max_threads) -> std::unique_ptr<IExecutor> {
    try {
        return std::make_unique<ThreadPoolExecutor>(current_threads, max_threads);
    } catch (const std::exception& exp) {
        std::cerr << "Failed to create ThreadPoolExecutor: " << exp.what() << '\n';
        return nullptr;
    } catch (...) {
        std::cerr << "Unknown error occurred while creating ThreadPoolExecutor\n";
        return nullptr;
    }
}

} // namespace stp
} // namespace rtl
