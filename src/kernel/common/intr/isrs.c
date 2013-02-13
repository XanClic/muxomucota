#include <compiler.h>
#include <isr.h>
#include <kassert.h>
#include <kmalloc.h>
#include <stddef.h>

#include <arch-constants.h>


struct isr
{
    struct isr *next;

    bool is_kernel;
    union
    {
        void (*kernel_handler)(void);
        process_t *process;
    };
};

static struct isr *handlers[IRQ_COUNT];


static void register_isr(int irq, struct isr *isr)
{
    kassert((irq >= 0) && (irq < IRQ_COUNT));

    struct isr *first;

    do
    {
        first = handlers[irq];
        isr->next = first;
    }
    while (unlikely(!__sync_bool_compare_and_swap(&handlers[irq], first, isr)));
}

void register_kernel_isr(int irq, void (*isr)(void))
{
    struct isr *nisr = kmalloc(sizeof(*nisr));

    nisr->is_kernel = true;
    nisr->kernel_handler = isr;

    register_isr(irq, nisr);
}


void register_user_isr(int irq, process_t *process)
{
    struct isr *nisr = kmalloc(sizeof(*nisr));

    nisr->is_kernel = false;
    nisr->process = process;

    register_isr(irq, nisr);
}


static volatile bool plz_dont_free = false;


void unregister_isr_process(process_t *process)
{
    for (int irq = 0; irq < IRQ_COUNT; irq++)
    {
        for (struct isr **isr = &handlers[irq]; *isr != NULL; isr = &(*isr)->next)
        {
            if (!(*isr)->is_kernel && ((*isr)->process == process))
            {
                struct isr *this_isr = *isr;
                *isr = (*isr)->next;

                while (unlikely(plz_dont_free));

                kfree(this_isr);
            }
        }
    }
}


void common_irq_handler(int irq)
{
    plz_dont_free = true;

    for (struct isr *isr = handlers[irq]; isr != NULL; isr = isr->next)
    {
        if (unlikely(isr->is_kernel))
            isr->kernel_handler();
        else if (likely((isr->process->status == PROCESS_ACTIVE) || (isr->process->status == PROCESS_DAEMON)))
        {
            // FIXME: Das funktioniert nur Race-Condition-frei, weil beide
            // kmallocs, die im Ausführungspfad liegen, weniger als eine Page
            // anfordern (und eine Page bekommt der VMM auch lockfrei hin).
            // Besser wäre es, den handlenden Prozess explizit einen Thread
            // abstellen zu lassen (bspw. sich selbst), der hier benutzt wird,
            // sodass hier kein neuer Prozess angelegt werden muss.
            popup(isr->process, -irq - 1, 0, NULL, 0, false);
        }
    }

    plz_dont_free = false;
}
