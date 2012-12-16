#include <ipc.h>


extern void (*popup_entries[])(void);


void popup_handler(int index, void (*handler)(void))
{
    // FIXME: Überlauf und so
    popup_entries[index] = handler;
}
