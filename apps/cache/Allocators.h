#pragma once

#include <array>
#include <vector>
#include <memory>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <exception>
#include <sys/mman.h>
#include "BitUtils.h"

#define DEBUGIT(...)
// #define DEBUGIT(fmt, ...) printf(fmt, __VA_ARGS__)

constexpr std::size_t large_page_size = 2 * 1024 * 1024;
constexpr std::size_t standard_page_size = 4 * 1024;

inline size_t round_size(size_t n, size_t size) {
    return (((n - 1) / size) + 1) * size;
}

struct MemBlock {
    void* ptr;
    size_t size;
};

template <class BlockAlloc>
struct memory_pool : public BlockAlloc {
    memory_pool() {
    }
    void init(const std::shared_ptr<BlockAlloc>& alloc, size_t bytes) {
        balloc = alloc;
        size = std::max(sizeof(void*), bytes);
        // bits = std::bit_width(bytes - 1);
        // size = size_t(1) << bits;
        head = nullptr;
        DEBUGIT("  initialized %lu bytes\n", size);
    }
    ~memory_pool() {
    }
    void* alloc() {
        DEBUGIT("    allocating %lu bytes\n", size);
        ++num_allocs;
        if (head != nullptr) {
            void* res = head;
            head = *((void**)res);
            return res;
        }
        if (spaceleft < size) {
            ++num_blocks_alloc;
            MemBlock block = balloc->allocate_block(size);
            DEBUGIT("        allocated block %ld bytes - %ld bytes %ld items\n", size,
                    block.size, block.size / size);
            spaceleft = block.size;
            spaceptr = (uint8_t*)block.ptr;
            if (spaceptr == nullptr) {
                fprintf(stderr, "Could not allocate memory \n");
                std::exit(1);
            }
        }
        void* res = spaceptr;
        spaceleft -= size;
        spaceptr += size;
        return res;
    }

    void free(void* ptr) {
        ++num_deallocs;
        *((void**)ptr) = head;
        head = ptr;
    }

    uint64_t num_allocs = 0;
    uint64_t num_deallocs = 0;
    uint64_t num_blocks_alloc = 0;
    void* head = nullptr;
    uint8_t* spaceptr = nullptr;
    size_t spaceleft = 0;
    size_t size = 0;
    std::shared_ptr<BlockAlloc> balloc;
};

template <typename T, typename BlockAllocator>
struct retail_allocator {
    using value_type = T;
    retail_allocator() {
        alloc = std::make_shared<BlockAllocator>();
        pool = std::make_shared<PoolArray>();
        for (size_t j = 0; j < pool->size(); ++j) {
            auto range = irange2<1>(j);
            size_t bytes = range.base + range.range;
            DEBUGIT("Initializing bank %ld with size %lu\n", j, bytes);
            (*pool)[j].init(alloc, bytes);
        }
    }
    retail_allocator(retail_allocator&&) = delete;
    retail_allocator(const retail_allocator&) = delete;
    template <typename U, typename B>
    constexpr retail_allocator(const retail_allocator<U, B>& rhs) noexcept
        : pool(rhs.pool), alloc(rhs.alloc) {
    }

    T* allocate(std::size_t n) {
        size_t bytes = std::max(sizeof(void*), n * sizeof(T));
        // int bank = std::bit_width(bytes);
        int bank = ilog2<1>(bytes);
        DEBUGIT("Allocating %ld items, %ld bytes each, total %ld bytes from bank %d\n", n,
                sizeof(T), bytes, bank);
        uint8_t* ptr = (uint8_t*)(*pool)[bank].alloc();
        if (ptr == nullptr) {
            fprintf(stderr, "Could not allocate %lu bytes bank %d \n", bytes, bank);
            std::exit(1);
        }
        return (T*)(ptr);
    }
    void deallocate(T* p, std::size_t n) {
        size_t bytes = std::max(sizeof(void*), n * sizeof(T));
        // int bank = std::bit_width(bytes);
        int bank = ilog2<1>(bytes);
        uint8_t* ptr = ((uint8_t*)p);
        (*pool)[bank].free(ptr);
    }
    using Pool = memory_pool<BlockAllocator>;
    using PoolArray = std::array<Pool, 64>;
    std::shared_ptr<BlockAllocator> alloc;
    std::shared_ptr<PoolArray> pool;
};

template <typename Derived>
struct base_allocator {
    std::vector<MemBlock> blocks;
    ~base_allocator() {
        for (MemBlock blk : blocks) {
            Derived* me = static_cast<Derived*>(this);
            me->free_block(blk.ptr, blk.size);
        }
    }
};

struct transparent_allocator : public base_allocator<transparent_allocator> {
    MemBlock allocate_block(std::size_t bytes) {
        size_t mapsize = round_size(bytes, large_page_size);
        void* p = nullptr;
        int result = posix_memalign(&p, large_page_size, mapsize);
        if (result != 0) {
            fprintf(stderr, "THP: could not allocate memory: %s\n", strerror(errno));
            throw std::bad_alloc();
        }
        result = madvise(p, mapsize, MADV_HUGEPAGE);
        if (result != 0) {
            fprintf(stderr, "THP: could not allocate memory: %s\n", strerror(errno));
            throw std::bad_alloc();
        }
        MemBlock block{p, mapsize};
        blocks.push_back(block);
        return block;
    }
    void free_block(void* p, std::size_t n) {
        std::free(p);
    }
};

struct hugepage_allocator : public base_allocator<hugepage_allocator> {
    MemBlock allocate_block(std::size_t bytes) {
        size_t mapsize = round_size(bytes, large_page_size);
        void* p = mmap(nullptr, mapsize, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        if (p == MAP_FAILED) {
            fprintf(stderr, "HUGE: could not allocate memory: %s\n", strerror(errno));
            throw std::bad_alloc();
        }
        MemBlock block{p, mapsize};
        blocks.push_back(block);
        return block;
    }

    void free_block(void* p, std::size_t bytes) {
        size_t mapsize = round_size(bytes, large_page_size);
        munmap(p, mapsize);
    }
};

struct mmap_allocator : public base_allocator<mmap_allocator> {
    MemBlock allocate_block(std::size_t bytes) {
        size_t mapsize = round_size(bytes, large_page_size);
        void* p = mmap(nullptr, mapsize, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) {
            fprintf(stderr, "MMAP: could not allocate memory: %s\n", strerror(errno));
            throw std::bad_alloc();
        }
        MemBlock block{p, mapsize};
        blocks.push_back(block);
        return block;
    }

    void free_block(void* p, std::size_t bytes) {
        size_t mapsize = round_size(bytes, large_page_size);
        munmap(p, mapsize);
    }
};

struct standard_allocator : public base_allocator<standard_allocator> {
    MemBlock allocate_block(std::size_t bytes) {
        size_t mapsize = round_size(bytes, large_page_size);
        void* p = ::malloc(mapsize);
        MemBlock block{p, mapsize};
        blocks.push_back(block);
        return block;
    }

    void free_block(void* p, std::size_t bytes) {
        ::free(p);
    }
};

#include <boost/container/pmr/unsynchronized_pool_resource.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>

boost::container::pmr::monotonic_buffer_resource pool(8 * 1024 * 1024ULL);
boost::container::pmr::unsynchronized_pool_resource buffer(&pool);

template <typename T>
struct BoostAllocator : public boost::container::pmr::polymorphic_allocator<T> {
    using allocator_type = boost::container::pmr::polymorphic_allocator<T>;
    BoostAllocator() : allocator_type(&buffer) {
    }
    BoostAllocator(BoostAllocator&&) = delete;
    BoostAllocator(const BoostAllocator&) = delete;
    template <typename U>
    BoostAllocator(const BoostAllocator<U>&) : allocator_type(&buffer) {
    }
};
