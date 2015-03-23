#include <compiler.hpp>
#include <cstddef>
#include <isr.hpp>
#include <kassert.hpp>
#include <kmalloc.hpp>
#include <lock.hpp>

#include <arch-constants.hpp>


using namespace mu;


struct isr {
    isr *next;

    bool is_kernel;
    union {
        void (*kernel_handler)(cpu_state *state);
        struct {
            process *proc;
            void (__attribute__((regparm(1))) *process_handler)(void *info);
            void *info;
        };
    };
};

static isr *handlers[IRQ_COUNT];


static void register_isr(int irq, struct isr *isr)
{
    kassert((irq >= 0) && (irq < IRQ_COUNT));

    struct isr *first;

    do {
        first = handlers[irq];
        isr->next = first;
    } while (unlikely(!__sync_bool_compare_and_swap(&handlers[irq], first, isr)));
}


void register_kernel_isr(int irq, void (*isr)(cpu_state *state))
{
    struct isr *nisr = new struct isr;

    nisr->is_kernel      = true;
    nisr->kernel_handler = isr;

    register_isr(irq, nisr);
}


template<> void register_user_isr(int irq, process *proc, void (__attribute__((regparm(1))) *process_handler)(void *info), void *info)
{
    kassert_print(proc->handled_irq < 0, "Already handling IRQ %i", proc->handled_irq);

    isr *nisr = new isr;

    nisr->is_kernel       = false;
    nisr->proc            = proc;
    nisr->process_handler = process_handler;
    nisr->info            = info;

    register_isr(irq, nisr);

    proc->handled_irq = irq;
}


static volatile bool plz_dont_free = false;


void unregister_isr_process(process *proc)
{
    for (int irq = 0; irq < IRQ_COUNT; irq++) {
        for (struct isr **isr = &handlers[irq]; *isr; isr = &(*isr)->next) {
            if (!(*isr)->is_kernel && ((*isr)->proc == proc)) {
                struct isr *this_isr = *isr;
                *isr = this_isr->next;

                while (unlikely(plz_dont_free));

                delete this_isr;

                // I don't understand this, but who cares
                if (!*isr) {
                    break;
                }
            }
        }
    }

    proc->handled_irq = -1;

    // TODO: Remove process from runqueue
}


extern lock runqueue_lock;


int common_irq_handler(int irq, cpu_state *state)
{
    int spawned_proc_count = 0;

    plz_dont_free = true;

    for (struct isr *isr = handlers[irq]; isr; isr = isr->next) {
        if (isr->is_kernel) {
            isr->kernel_handler(state);
        } else if (likely(__sync_bool_compare_and_swap(&isr->proc->status, process::DAEMON, process::COMA))) {
            isr->proc->handling_irq = true;
            isr->proc->fresh_irq = true;

            // This works without race conditions, because the process was a
            // daemon which is not in the runqueue. Therefore, there is no other
            // state which might be destroyed here.
            isr->proc->make_entry(isr->proc->cpu, reinterpret_cast<void (*)(void)>(isr->process_handler), isr->proc->irq_stack_top);

            // Has to be given by register, otherwise there may have to be locks
            // on the virtual memory
            process::set_func_regparam(isr->proc->cpu, reinterpret_cast<uintptr_t>(isr->info));

            // Shouldn't be necessary for register calling
            //process_simulate_func_call(isr->proc->cpu);

            isr->proc->status = process::ACTIVE;

            spawned_proc_count++;
        }
    }

    plz_dont_free = false;

    return spawned_proc_count;
}
