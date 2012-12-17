#include <ipc.h>


extern uintptr_t (*popup_entries[])(void);


void popup_handler(int index, uintptr_t (*handler)(void))
{
    // FIXME: Überlauf und so
    popup_entries[index] = handler;
}
