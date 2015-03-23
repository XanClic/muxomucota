#ifndef _PROCESS_HPP
#define _PROCESS_HPP

#include <climits>
#include <cstddef>
#include <cstdint>
#include <shm.hpp>
#include <tls.hpp>
#include <vmem.hpp>
#include <voodoo>
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


struct popup_return_memory: public kernel_object {
    popup_return_memory(void): kernel_object(sizeof(*this)) {}

    pid_t caller;

    size_t size;
    void *mem;
};


struct process: process_arch_info {
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


    typedef void (*popup_entry_t)(void);
    s_attribute(process, popup_entry_t, popup_entry);


    void arch_switch_to(void);


    process(const char *name);

    static process *kernel_thread(const char *name, void (*function)(void));

    static process *user_thread(void (*function)(void *), void *stack_top, void *arg);
    static process *user_thread(std::nullptr_t, void *stack_top, std::nullptr_t);

    template<typename T> static process *user_thread(void (*function)(T *), void *stack_top, T *arg)
    {
        return user_thread(reinterpret_cast<void (*)(void *)>(function), stack_top, arg);
    }

    static process *find(pid_t pid);


    void destroy(uintmax_t exit_info);


    void run(void);
    void daemonize(const char *name);
    void daemonize_from_irq_handler(void);

    pid_t wait(uintmax_t *status, int options, int *errnop, popup_return_memory **prmp);

    void make_entry(cpu_state *state, void (*address)(void), void *stack);

    void add_func_param(cpu_state *state, uintptr_t param);
    static void set_func_regparam(cpu_state *state, uintptr_t param);
    static void simulate_func_call(cpu_state *state);


    pid_t popup(int func_index, shm_reference shm, const void *buffer, size_t length, bool zombify);


    bool is_popup(void);


    private:
        void set_popup_entry(popup_entry_t entry);
};


extern process *current_process;


cpu_state *dispatch(cpu_state *state, pid_t switch_to);
process *schedule(pid_t schedule_to);

int exec(cpu_state *state, const void *file, size_t file_length, char *const *argv, char *const *envp);

#endif
