#include <ipc.h>


extern uintmax_t (*popup_entries[])(uintptr_t);


void popup_message_handler(int index, uintmax_t (*handler)(void))
{
    // FIXME: Überlauf und so
    popup_entries[index] = (uintmax_t (*)(uintptr_t))handler;
}


void popup_shm_handler(int index, uintmax_t (*handler)(uintptr_t))
{
    popup_entries[index] = handler;
}
