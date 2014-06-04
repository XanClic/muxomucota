#ifndef _PROCESS_HPP
#define _PROCESS_HPP

#include <climits>
#include <cstddef>
#include <cstdint>
#include <tls.hpp>
#include <vmem.hpp>
#include <sys/types.h>

#include <arch-process.hpp>


struct cpu_state;


#define MAX_PID_COUNT  65536
#define POPUP_STACKS    1024
#define POP_STACK_SIZE 65536


struct popup_stack_mask {
    int refcount;
    unsigned long mask[POPUP_STACKS / LONG_BIT];
};


struct popup_return_memory {
    pid_t caller;

    size_t size;
    void *mem;
};


struct process {
    pid_t pid, pgid;
    volatile pid_t ppid;

    volatile enum {
        ACTIVE,
        COMA,
        ZOMBIE,
        DAEMON,
        DESTRUCT
    } status;

    cpu_state *cpu;
    vmm_context *vmm;

    process *volatile rq_next; // runqueue next
    process *volatile next;

    unsigned schedule_tick;

    char name[32]; // FIXME

    struct tls *tls;

    void (*popup_entry)(void);
    popup_stack_mask *psm;

    int psi; // popup stack index

    void *ptmp; // popup tmp
    size_t ptmpsz;

    popup_return_memory *prm;

    bool pzomb; // popup zombify

    uintmax_t exit_info;

    int handled_irq;
    bool fresh_irq, handling_irq;
    volatile int settle_count;

    void *irq_stack_top;


    void arch_switch_to(void);


private:
    process_arch_info arch;
};


extern process *current_process;


cpu_state *dispatch(cpu_state *state, pid_t switch_to);
process *schedule(pid_t schedule_to);

#endif
