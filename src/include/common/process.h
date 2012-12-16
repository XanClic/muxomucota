#ifndef _PROCESS_H
#define _PROCESS_H

#include <arch-process.h>
#include <stddef.h>
#include <stdint.h>
#include <vmem.h>
#include <sys/types.h>


struct cpu_state;


typedef enum process_status
{
    PROCESS_ACTIVE,
    PROCESS_COMA,
    PROCESS_ZOMBIE,
    PROCESS_DAEMON,
    PROCESS_DESTRUCT
} process_status_t;


#define POPUP_STACKS     1024
#define POPUP_STACK_SIZE 4096


typedef struct process
{
    pid_t pid; // process ID

    pid_t pgid; // process group ID

    process_status_t status;

    struct cpu_state *cpu_state;
    vmm_context_t *vmmc;

    struct process *runqueue_next;

    struct process_arch_info arch;

    char name[32]; // FIXME

    int *errno;

    void (*popup_entry)(void);
    uint_fast32_t *popup_stack_mask;

    int popup_stack_index;
} process_t;


extern process_t *current_process;


void start_doing_stuff(void);


struct cpu_state *dispatch(struct cpu_state *state);
void arch_process_change(process_t *new_process);

process_t *schedule(void);


process_t *create_empty_process(const char *name);

void make_idle_process(void);


void make_process_entry(process_t *proc, void (*address)(void), void *stack);
void alloc_cpu_state(process_t *proc);
void initialize_cpu_state(struct cpu_state *state, void (*entry)(void), void *stack);

void register_process(process_t *proc);

void set_process_popup_entry(process_t *proc, void (*entry)(void));


process_t *find_process(pid_t pid);


int popup(process_t *proc);


pid_t create_process_from_image(const char *name, const void *address, size_t size);


void daemonize_process(process_t *proc);


void destroy_process_struct(process_t *proc);
void destroy_process_arch_struct(process_t *proc);

void destroy_process(process_t *proc);

void destroy_this_popup_thread(void);

#endif
