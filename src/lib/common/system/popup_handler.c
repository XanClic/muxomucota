#include <ipc.h>


extern uintptr_t (*popup_entries[])(uintptr_t);


void popup_message_handler(int index, uintptr_t (*handler)(void))
{
    // FIXME: Überlauf und so
    popup_entries[index] = (uintptr_t (*)(uintptr_t))handler;
}


void popup_shm_handler(int index, uintptr_t (*handler)(uintptr_t))
{
    popup_entries[index] = handler;
}
