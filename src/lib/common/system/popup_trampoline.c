#include <ipc.h>
#include <stdint.h>
#include <stddef.h>


uintptr_t (*popup_entries[MAX_POPUP_HANDLERS])(void);


void _popup_ll_trampoline(uintptr_t func_index) __attribute__((weak));

void _popup_ll_trampoline(uintptr_t func_index)
{
    uintptr_t retval = 0;

    if (popup_entries[func_index] != NULL)
        retval = popup_entries[func_index]();

    popup_exit(retval);
}


void _popup_trampoline(int func_index);

void _popup_trampoline(int func_index)
{
    uintptr_t retval = 0;

    if (popup_entries[func_index] != NULL)
        retval = popup_entries[func_index]();

    popup_exit(retval);
}
