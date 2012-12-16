#include <isr.h>
#include <kassert.h>
#include <stddef.h>

#include <arch-constants.h>


// TODO: Linked List für die ISRs
static struct cpu_state *(*handlers[IRQ_COUNT])(int irq, struct cpu_state *state);


void register_isr(int irq, struct cpu_state *(*isr)(int irq, struct cpu_state *state))
{
    kassert((irq >= 0) && (irq < IRQ_COUNT));
    kassert(handlers[irq] == NULL);

    handlers[irq] = isr;
}


struct cpu_state *common_irq_handler(int irq, struct cpu_state *state)
{
    return (handlers[irq] == NULL) ? state : handlers[irq](irq, state);
}
