/************************************************************************
 * Copyright (C) 2013 by Hanna Reitz                                    *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

#ifndef _PROCESS_H
#define _PROCESS_H

#include <arch-process.h>
#include <cpu-state.h>
#include <digest.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <tls.h>
#include <vmem.h>
#include <sys/types.h>


#ifdef KERNEL
// #define COOPERATIVE
#endif


struct cpu_state;


typedef enum process_status
{
    PROCESS_ACTIVE,
    PROCESS_COMA,
    PROCESS_ZOMBIE,
    PROCESS_DAEMON,
    PROCESS_DESTRUCT
} process_status_t;


#define MAX_PID_COUNT    65536

#define POPUP_STACKS      1024
#define POPUP_STACK_SIZE 65536


typedef struct
{
    int refcount;
    unsigned long mask[POPUP_STACKS / LONG_BIT];
} popup_stack_mask_t;


typedef struct
{
    // Wer erlaubt ist, diesen Speicher auszulesen (der Prozess, der den
    // Popup-Thread gestartet hat)
    pid_t parent;

    size_t size;
    void *mem;

    DIGESTIFY
} popup_return_memory_t;


typedef struct process
{
    pid_t pid; // process ID

    pid_t pgid; // process group ID

    volatile process_status_t status;

    struct cpu_state *cpu_state;
    vmm_context_t *vmmc;

    // rq_next = runqueue_next
    struct process *volatile rq_next, *volatile next;

    struct process_arch_info arch;

    unsigned schedule_tick;


    char name[32]; // FIXME
    // 32 Zeichen pro Eintrag
    char *aliases;
    int alias_count;

    struct tls *tls;


    volatile pid_t ppid;


    // TODO: Ein paar Unions
    void (*popup_entry)(void);
    popup_stack_mask_t *popup_stack_mask;

    int popup_stack_index;

    void *popup_tmp;
    size_t popup_tmp_sz;

    popup_return_memory_t *popup_return;

    bool popup_zombify;


    uintmax_t exit_info;


    int handled_irq;
    bool fresh_irq, handling_irq;
    volatile int settle_count;

    void *irq_stack_top;
} process_t;


extern process_t *current_process;


struct cpu_state *dispatch(struct cpu_state *state, pid_t switch_to);
void arch_process_change(process_t *new_process);

process_t *schedule(pid_t schedule_to);


process_t *create_empty_process(const char *name);
process_t *create_kernel_thread(const char *name, void (*function)(void));
process_t *create_user_thread(void (*function)(void *), void *stack_top, void *arg);

void make_idle_process(void);

void enter_idle_process(void);


void make_process_entry(process_t *proc, struct cpu_state *state, void (*address)(void), void *stack);

void process_create_basic_mappings(process_t *proc);

void process_set_args(process_t *proc, struct cpu_state *state, int argc, const char *const *argv, int envc, const char *const *envp);

void alloc_cpu_state(process_t *proc);
void alloc_cpu_state_on_stack(process_t *proc, void *stack, size_t stacksz);
void initialize_cpu_state(process_t *proc, struct cpu_state *state, void (*entry)(void), void *stack, int parcount, ...);
void initialize_kernel_thread_cpu_state(process_t *proc, struct cpu_state *state, void (*entry)(void));
void initialize_forked_cpu_state(struct cpu_state *dest, const struct cpu_state *src);

void initialize_child_process_arch(process_t *child, process_t *parent);
void initialize_orphan_process_arch(process_t *proc);

void process_set_initial_params(process_t *proc, struct cpu_state *state, int argc, const char *const *argv, const char *const *envp);

void *process_stack_alloc(struct cpu_state *state, size_t size);


void add_process_func_param(process_t *proc, struct cpu_state *state, uintptr_t param);
void set_process_func_regparam(struct cpu_state *state, uintptr_t param);
void process_simulate_func_call(struct cpu_state *state);


void run_process(process_t *proc);

void register_process(process_t *proc);


void set_process_popup_entry(process_t *proc, void (*entry)(void));


process_t *find_process(pid_t pid);


int exec(struct cpu_state *state, const void *file, size_t file_length, char *const *argv, char *const *envp);


pid_t popup(process_t *proc, int func_index, uintptr_t shmid, const void *buffer, size_t length, bool zombify);

bool is_popup(process_t *proc);

bool check_process_file_image(const void *address);

pid_t create_process_from_image(int argc, const char *const *argv, const void *address);
bool load_image_to_process(process_t *proc, const void *address, void **heap_start, void (**entry)(void));


void process_add_alias(process_t *proc, const char *alias);
void daemonize_process(process_t *proc, const char *name);
void daemonize_from_irq_handler(process_t *proc);


void destroy_process_struct(process_t *proc);
void destroy_process_arch_struct(process_t *proc);

void destroy_process(process_t *proc, uintmax_t exit_info);

void destroy_this_popup_thread(uintmax_t exit_info);


pid_t raw_waitpid(pid_t pid, uintmax_t *status, int options, int *errnop, popup_return_memory_t **prmp);


void sweep_dead_processes(void);


#ifdef X86
void kiopl(int level, struct cpu_state *state);
#endif

#endif
