#include <ipc.h>
#include <stdint.h>
#include <stddef.h>


uintmax_t (*popup_entries[MAX_POPUP_HANDLERS])(uintptr_t);


// FIXME: Kann man das Symbol nicht weak auf die gleiche Adresse wie
// _popup_trampoline() legen?
void _popup_ll_trampoline(uintptr_t func_index, uintptr_t shmid) __attribute__((weak));

void _popup_ll_trampoline(uintptr_t func_index, uintptr_t shmid)
{
    uintmax_t retval = 0;

    if (popup_entries[func_index] != NULL)
        retval = popup_entries[func_index](shmid);

    popup_exit(retval);
}


void _popup_trampoline(int func_index, uintptr_t shmid);

void _popup_trampoline(int func_index, uintptr_t shmid)
{
    uintmax_t retval = 0;

    if (popup_entries[func_index] != NULL)
        retval = popup_entries[func_index](shmid);

    popup_exit(retval);
}
