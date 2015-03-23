#include <cstdint>
#include <kassert.hpp>
#include <kmalloc.hpp>
#include <parith.hpp>
#include <pmm.hpp>
#include <vmem.hpp>


void *kmalloc(size_t size)
{
    size += sizeof(size);

    int frame_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t pageframes[frame_count];

    // Avoid external fragmentation
    for (int i = 0; i < frame_count; i++) {
        pageframes[i] = physptr::alloc();
    }

    void *mem = kernel_map(pageframes, sizeof(pageframes) / sizeof(pageframes[0]));
    *static_cast<size_t *>(mem) = size;

    return ptr_diff_arith<size_t>(mem) + 1;
}


void kfree(void *ptr)
{
    if (!ptr) {
        return;
    }

    ptr = ptr_diff_arith<size_t>(ptr) - 1;
    kassert_print(!(reinterpret_cast<uintptr_t>(ptr) & PAGE_MASK), "ptr=%p", ptr);

    size_t size = *static_cast<size_t *>(ptr);

    int frame_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (int i = 0; i < frame_count; i++) {
        void *address = byte_ptr(ptr) + static_cast<uintptr_t>(i) * PAGE_SIZE;
        --physptr(kernel_unmap(address, PAGE_SIZE)).users;
    }
}
