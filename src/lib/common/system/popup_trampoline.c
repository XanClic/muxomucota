#include <ipc.h>
#include <stdint.h>
#include <stddef.h>


uintmax_t (*popup_entries[MAX_POPUP_HANDLERS])(uintptr_t);
void (*popup_irq_entries[MAX_IRQ_HANDLERS])(void);


inline void _popup_trampoline(int func_index, uintptr_t shmid);


void _popup_ll_trampoline(uintptr_t func_index, uintptr_t shmid) __attribute__((weak));

void _popup_ll_trampoline(uintptr_t func_index, uintptr_t shmid)
{
    _popup_trampoline((int)func_index, shmid);
}


void _popup_trampoline(int func_index, uintptr_t shmid)
{
    uintmax_t retval = 0;

    if (func_index >= 0)
    {
        if (popup_entries[func_index] != NULL)
            retval = popup_entries[func_index](shmid);
    }
    else if (popup_irq_entries[-func_index - 1] != NULL)
        popup_irq_entries[-func_index - 1]();

    popup_exit(retval);
}
