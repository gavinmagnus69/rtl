#ifndef rtl_stp_blockallocator_hpp
#define rtl_stp_blockallocator_hpp

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <format>
#include <iostream>
#include <memory>
#include <vector>


namespace rtl {


struct AllocatorBlock {
    size_t block_size{0};
    void* block_ptr{nullptr};
    AllocatorBlock() = default;
    ~AllocatorBlock() = default;
};


static constexpr size_t BlocksSize = 1024;
static constexpr size_t LINEAR_ALLOCATOR_BLOCK_SIZE = 1024 * 1024; // 1 MB


template <typename T>
class BlockAllocator {
public:
    using value_type = T;
    BlockAllocator() {
        std::cout << "Default constructor\n";
        m_buffer = std::make_shared<std::array<unsigned char, LINEAR_ALLOCATOR_BLOCK_SIZE>>(); // allocating 1 MB block on the heap in the vector
        m_allocatedBlocks = std::make_shared<std::array<AllocatorBlock, BlocksSize>>();
        m_freeBlocks = std::make_shared<std::array<AllocatorBlock, BlocksSize>>();
        m_freeBlocks->at(0).block_ptr = (void*)m_buffer->data();
        m_freeBlocks->at(0).block_size = LINEAR_ALLOCATOR_BLOCK_SIZE;
    };


    BlockAllocator(const BlockAllocator& other) noexcept
        : m_allocatedBlocks(other.m_allocatedBlocks)
        , m_freeBlocks(other.m_freeBlocks)
        , m_buffer(other.m_buffer) {
    }


    ~BlockAllocator() = default;
public:
    T* allocate(size_t n) {
        std::cout << std::format("trying to allocate {} elements of {} bytes\n", n, sizeof(value_type) * n);
        T* ptr = allocateToFreeBlock(n);
        mergeFreeBlocks();
        // assert(ptr == nullptr && "Something happened");
        assert(ptr != nullptr && "Nullptr");
        printAllocatedBlocks();
        printFreeBlocks();
        return ptr;
    }

    void deallocate(T* p, std::size_t n) noexcept {
        printAllocatedBlocks();
        printFreeBlocks();
        std::cout << std::format("trying to deallocate {} elements of {} bytes\n", n, sizeof(value_type) * n);
        auto& freeBlock = searchNullptrInFreeBlock();
        for (auto& block : *m_allocatedBlocks) {
            // writing allocated block to free block
            if (block.block_ptr == (void*)p) {
                freeBlock.block_ptr = (void*)p;
                freeBlock.block_size = block.block_size;
                mergeFreeBlocks();
                return;
            }
        }
    }
private:
    T* allocateToFreeBlock(size_t size) {
        for (auto& block : *m_freeBlocks) {
            // success case (free block founded)
            if (block.block_ptr != nullptr && block.block_size >= size * sizeof(value_type)) {
                T* ptr = reinterpret_cast<T*>(block.block_ptr); // pointer to occupy
                auto& blockToAllocate = searchNullptrInAllocatedBlock();
                blockToAllocate.block_ptr = block.block_ptr;
                blockToAllocate.block_size = size * sizeof(value_type);
                // bytes that left after occupying free block
                if (size_t bytesLeft = block.block_size - size * sizeof(value_type); bytesLeft > 0) {
                    block.block_ptr = (T*)block.block_ptr + size; // pointer arithmetics
                    block.block_size = bytesLeft;
                } else {
                    makeBlockNullptr(block);
                }
                assert(ptr != nullptr && "Ptr is nullptr");
                // what if we consume all free space in block
                return ptr;
            }
        }
        // means no free block
        assert(false && "No free blocks available");
        return nullptr; // if no free block
    }

    AllocatorBlock& searchNullptrInFreeBlock() {
        for (auto& block : *m_freeBlocks) {
            if (block.block_ptr == nullptr) {
                return block;
            }
        }
        assert(false && "failed to find nullptr free block");
        return m_allocatedBlocks->at(0);
    }

    AllocatorBlock& searchNullptrInAllocatedBlock() {
        for (auto& block : *m_allocatedBlocks) {
            if (block.block_ptr == nullptr) {
                return block;
            }
        }
        assert(false && "No free blocks");
        return m_allocatedBlocks->at(0);
    }

    void makeBlockNullptr(AllocatorBlock& block) {
        block.block_ptr = nullptr;
        block.block_size = 0;
    }

    void printAllocatedBlocks() {
        std::cout << "Allocated blocks:\n";
        for (const auto& block : *m_allocatedBlocks) {
            if (block.block_ptr != nullptr) {
                printBlock(block);
            }
        }
    }

    void printFreeBlocks() {
        std::cout << "Free blocks:\n";
        for (const auto& block : *m_freeBlocks) {
            if (block.block_ptr != nullptr) {
                if (block.block_ptr != nullptr) {
                    printBlock(block);
                }
            }
        }
    }

    void printBlock(const AllocatorBlock& block) {
        std::cout << std::hex << block.block_ptr << ", address in dec: " << std::dec << block.block_ptr << ", block size: " << block.block_size << '\n';
    }
// TODO: not optimal implementation, needs improvement
    void mergeFreeBlocks() {
        std::cout << "Merging free blocks:\n";
        printFreeBlocks();
        std::sort(m_freeBlocks->begin(), m_freeBlocks->end(), [](const AllocatorBlock& left, const AllocatorBlock& right) { return std::less<void>{}(left.block_ptr, right.block_ptr); });
        for (size_t i = 0; i < BlocksSize - 1; ++i) {
            auto& currentBlock = m_freeBlocks->at(i);
            auto& nextBlock = m_freeBlocks->at(i + 1);
            if (currentBlock.block_ptr != nullptr && nextBlock.block_ptr != nullptr) {
                unsigned char* currentEndPtr = reinterpret_cast<unsigned char*>(currentBlock.block_ptr) + currentBlock.block_size;
                if (currentEndPtr == nextBlock.block_ptr) {
                    // merging
                    currentBlock.block_size += nextBlock.block_size;
                    makeBlockNullptr(nextBlock);
                }
            }
        }
        std::cout << "after merging:\n";
        printFreeBlocks();
    }
private:
    std::shared_ptr<std::array<AllocatorBlock, BlocksSize>> m_allocatedBlocks{nullptr};
    std::shared_ptr<std::array<AllocatorBlock, BlocksSize>> m_freeBlocks{nullptr}; // free blocks are all
    std::shared_ptr<std::array<unsigned char, LINEAR_ALLOCATOR_BLOCK_SIZE>> m_buffer{nullptr};
};

}; // namespace rtl

#endif
