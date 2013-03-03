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
        void (*kernel_handler)(struct cpu_state *state);
        struct
        {
            process_t *process;
            void (*process_handler)(void *info);
            void *info;
        };
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

void register_kernel_isr(int irq, void (*isr)(struct cpu_state *state))
{
    struct isr *nisr = kmalloc(sizeof(*nisr));

    nisr->is_kernel = true;
    nisr->kernel_handler = isr;

    register_isr(irq, nisr);
}


void register_user_isr(int irq, process_t *process, void (*process_handler)(void *info), void *info)
{
    struct isr *nisr = kmalloc(sizeof(*nisr));

    nisr->is_kernel = false;
    nisr->process = process;
    nisr->process_handler = process_handler;
    nisr->info            = info;

    register_isr(irq, nisr);


    process->handles_irqs = true;
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
                *isr = this_isr->next;

                while (unlikely(plz_dont_free));

                kfree(this_isr);

                // Versteh ich nicht. Egal.
                if (*isr == NULL)
                    break;
            }
        }
    }


    process->handles_irqs = false;

    // TODO: Prozess aus der Runqueue entfernen
}


extern lock_t runqueue_lock;


int common_irq_handler(int irq, struct cpu_state *state)
{
    int spawned_proc_count = 0;

    plz_dont_free = true;

    for (struct isr *isr = handlers[irq]; isr != NULL; isr = isr->next)
    {
        if (isr->is_kernel)
            isr->kernel_handler(state);
        else if (likely(__sync_bool_compare_and_swap(&isr->process->status, PROCESS_DAEMON, PROCESS_COMA)))
        {
            isr->process->currently_handled_irq = irq;
            isr->process->fresh_irq = true;

            kassert(isr->process->pid != isr->process->pgid);

            // Das funktioniert ohne jegliche Race Conditions, weil der Prozess
            // ein Daemon war, der sich nicht in der Runqueue befindet. Somit
            // gibt es auch keinen State, den man hier kaputtmachen könnte.
            make_process_entry(isr->process, isr->process->cpu_state, (void (*)(void))isr->process_handler, isr->process->irq_stack_top);

            add_process_func_param(isr->process, isr->process->cpu_state, (uintptr_t)isr->info);
            process_simulate_func_call(isr->process->cpu_state);

            isr->process->status = PROCESS_ACTIVE;

            spawned_proc_count++;
        }
    }

    plz_dont_free = false;

    return spawned_proc_count;
}
