#include <shm.h>
#include <stdint.h>
#include <syscall.h>


uintptr_t shm_make(int count, void **vaddr_list, int *page_count_list)
{
    return syscall3(SYS_SHM_MAKE, count, (uintptr_t)vaddr_list, (uintptr_t)page_count_list);
}
