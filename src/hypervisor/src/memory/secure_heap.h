#pragma once
#include <cstdint>
#include <atomic>
#include "../core/definitions.h"

class SecureHeapManager
{
public:
    struct HeapStats
    {
        std::atomic<std::uint64_t> total_allocated;
        std::atomic<std::uint64_t> total_freed;
        std::atomic<std::uint64_t> current_usage;
        std::atomic<std::uint32_t> allocation_count;
        std::atomic<std::uint32_t> free_count;
    };

    enum class AllocationFlags : std::uint32_t
    {
        None = 0,
        ZeroMemory = 1 << 0,
        NonPaged = 1 << 1,
        ExecuteRead = 1 << 2,
        NoExecute = 1 << 3,
        CacheAligned = 1 << 4,
        Encrypted = 1 << 5,
        Hidden = 1 << 6  // Hide from PTE walking
    };

    static bool initialize(void* heap_base, std::size_t heap_size);
    static void cleanup();

    // Thread-safe allocation functions
    static void* allocate(std::size_t size, AllocationFlags flags = AllocationFlags::ZeroMemory);
    static void* allocate_aligned(std::size_t size, std::size_t alignment, AllocationFlags flags = AllocationFlags::ZeroMemory);
    static void free(void* ptr);
    
    // Pool allocators for common sizes
    static void* allocate_from_pool(std::size_t size);
    static void return_to_pool(void* ptr, std::size_t size);
    
    // Security functions
    static void encrypt_block(void* ptr, std::size_t size);
    static void decrypt_block(void* ptr, std::size_t size);
    static void secure_zero(void* ptr, std::size_t size);
    
    // Anti-detection functions
    static void hide_allocation_from_pte(void* ptr, std::size_t size);
    static void reveal_allocation_in_pte(void* ptr, std::size_t size);
    static void scramble_heap_metadata();
    
    // Statistics
    static HeapStats get_stats() { return stats_; }
    
private:
    struct BlockHeader
    {
        std::uint32_t magic;
        std::uint32_t size;
        std::uint32_t flags;
        std::uint32_t checksum;
        BlockHeader* next;
        BlockHeader* prev;
        std::uint64_t allocation_time;
        void* return_address;  // For tracking allocations
    };

    struct MemoryPool
    {
        std::size_t block_size;
        std::uint32_t total_blocks;
        std::uint32_t free_blocks;
        void* pool_base;
        std::uint64_t* free_list_bitmap;
        std::atomic_flag lock = ATOMIC_FLAG_INIT;
    };

    static constexpr std::uint32_t HEAP_MAGIC = 0x48454150; // 'HEAP'
    static constexpr std::uint32_t FREE_MAGIC = 0x46524545; // 'FREE'
    static constexpr std::uint32_t POOL_COUNT = 8;
    
    static void* heap_base_;
    static std::size_t heap_size_;
    static BlockHeader* free_list_;
    static std::atomic_flag heap_lock_;
    static HeapStats stats_;
    static MemoryPool pools_[POOL_COUNT];
    
    // XOR key for simple encryption
    static std::uint64_t xor_key_;
    
    // Internal functions
    static BlockHeader* find_free_block(std::size_t size);
    static void split_block(BlockHeader* block, std::size_t size);
    static void coalesce_free_blocks();
    static std::uint32_t calculate_checksum(BlockHeader* header);
    static bool validate_header(BlockHeader* header);
    
    // Pool sizes: 64, 128, 256, 512, 1024, 2048, 4096, 8192
    static constexpr std::size_t pool_sizes_[POOL_COUNT] = {
        64, 128, 256, 512, 1024, 2048, 4096, 8192
    };
};

// Helper class for automatic cleanup
class SecureAllocation
{
public:
    SecureAllocation(std::size_t size, SecureHeapManager::AllocationFlags flags = SecureHeapManager::AllocationFlags::ZeroMemory)
        : ptr_(SecureHeapManager::allocate(size, flags)), size_(size)
    {
    }
    
    ~SecureAllocation()
    {
        if (ptr_)
        {
            SecureHeapManager::secure_zero(ptr_, size_);
            SecureHeapManager::free(ptr_);
        }
    }
    
    // Disable copy
    SecureAllocation(const SecureAllocation&) = delete;
    SecureAllocation& operator=(const SecureAllocation&) = delete;
    
    // Enable move
    SecureAllocation(SecureAllocation&& other) noexcept
        : ptr_(other.ptr_), size_(other.size_)
    {
        other.ptr_ = nullptr;
        other.size_ = 0;
    }
    
    SecureAllocation& operator=(SecureAllocation&& other) noexcept
    {
        if (this != &other)
        {
            if (ptr_)
            {
                SecureHeapManager::secure_zero(ptr_, size_);
                SecureHeapManager::free(ptr_);
            }
            ptr_ = other.ptr_;
            size_ = other.size_;
            other.ptr_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }
    
    void* get() { return ptr_; }
    const void* get() const { return ptr_; }
    std::size_t size() const { return size_; }
    
    template<typename T>
    T* as() { return static_cast<T*>(ptr_); }
    
    template<typename T>
    const T* as() const { return static_cast<const T*>(ptr_); }
    
private:
    void* ptr_;
    std::size_t size_;
};
