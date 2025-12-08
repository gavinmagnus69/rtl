#ifndef rtl_stp_linearallocator_hpp
#define rtl_stp_linearallocator_hpp


#include <array>
#include <atomic>
#include <cassert>
#include <format>
#include <iostream>
#include <memory>
#include <vector>


namespace rtl {


static constexpr size_t ALLOCATOR_SIZE = 1024 * 1024; // 1 MB


template <typename T>
class LinearAllocator {
public:
    using value_type = T;

    LinearAllocator() {
        m_offset = std::make_shared<std::atomic<size_t>>(0);
        m_buffer = std::make_shared<std::vector<unsigned char>>(ALLOCATOR_SIZE); // allocating 1 MB block on the heap in the vector
    }
    LinearAllocator(const LinearAllocator& other) noexcept
        : m_buffer(other.m_buffer)
        , m_offset(other.m_offset) {
    }

    ~LinearAllocator() = default;

    T* allocate(size_t n) {
        std::cout << std::format("trying to allocate {} elements of {} bytes\n", n, sizeof(value_type) * n);
        assert(n > 0 && n * sizeof(value_type) <= ALLOCATOR_SIZE);
        if ((m_offset->load() + (n * sizeof(value_type))) > ALLOCATOR_SIZE) {
            throw std::bad_alloc(); // out of memory
        }
        std::cout << std::format("allocated {} bytes at offset {}\n", n * sizeof(value_type), m_offset->load());
        T* ptr = reinterpret_cast<T*>(m_buffer->data() + m_offset->fetch_add(n * sizeof(value_type)));
        std::cout << std::format("allocated {} bytes after offset {}\n", n * sizeof(value_type), m_offset->load());
        // m_offset += n * sizeof(value_type);
        return ptr;
    }

    void deallocate(T* p, std::size_t n) noexcept {
        std::cout << "LinearAllocator deallocate called, but no action taken (linear allocator does not support deallocation of individual elements)\n";
    }
private:
    std::shared_ptr<std::atomic<size_t>> m_offset{nullptr}; //offset in bytes
    std::shared_ptr<std::vector<unsigned char>> m_buffer;
};
}; // namespace rtl
#endif