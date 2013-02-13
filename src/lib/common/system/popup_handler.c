#include <ipc.h>
#include <syscall.h>


extern uintmax_t (*popup_entries[])(uintptr_t);
extern void (*popup_irq_entries[])(void);


void popup_message_handler(int index, uintmax_t (*handler)(void))
{
    // FIXME: Überlauf und so
    popup_entries[index] = (uintmax_t (*)(uintptr_t))handler;
}


void popup_shm_handler(int index, uintmax_t (*handler)(uintptr_t))
{
    popup_entries[index] = handler;
}


void popup_irq_handler(int irq, void (*handler)(void))
{
    popup_irq_entries[irq] = handler;

    syscall1(SYS_HANDLE_IRQ, irq);
}
