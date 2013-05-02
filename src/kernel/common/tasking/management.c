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

#include <arch-constants.h>
#include <cpu-state.h>
#include <errno.h>
#include <ipc.h>
#include <isr.h>
#include <kassert.h>
#include <kmalloc.h>
#include <limits.h>
#include <lock.h>
#include <process.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <vmem.h>
#include <sys/types.h>
#include <sys/wait.h>


extern lock_t schedule_lock;

process_t *current_process, *idle_process, *runqueue, *daemons, *zombies, *dead;


static unsigned long used_pids[MAX_PID_COUNT / LONG_BIT];


static inline pid_t get_new_pid(void)
{
    for (int i = 0; i < MAX_PID_COUNT/ LONG_BIT; i++)
    {
        if (used_pids[i] != ULONG_MAX)
        {
            int bit = ffsl(~used_pids[i]);

            unsigned long old = used_pids[i];
            unsigned long new = used_pids[i] | (1 << (bit - 1));

            if (!__sync_bool_compare_and_swap(&used_pids[i], old, new))
            {
                i--;
                continue;
            }


            return (pid_t)i * LONG_BIT + bit;
        }
    }


    kassert_print(0, "PIDs exhausted (max is %i)", MAX_PID_COUNT);
}


static inline void relinquish_pid(pid_t pid)
{
    unsigned long new, old;

    do
    {
        old = used_pids[(pid - 1) / LONG_BIT];
        new = old & ~(1 << ((pid - 1) % LONG_BIT));
    }
    while (!__sync_bool_compare_and_swap(&used_pids[(pid - 1) / LONG_BIT], old, new));
}


process_t *create_empty_process(const char *name)
{
    process_t *p = kcalloc(sizeof(*p));

    p->status = PROCESS_COMA;

    p->pgid = p->pid = get_new_pid();
    p->ppid = (current_process == NULL) ? 0 : current_process->pid;

    strncpy(p->name, name, sizeof(p->name) - 1);

    p->popup_stack_index = -1;
    p->handled_irq = -1;


    initialize_orphan_process_arch(p);

    alloc_cpu_state(p);


    p->vmmc = kmalloc(sizeof(*p->vmmc));

    create_vmm_context(p->vmmc);
    use_vmm_context(p->vmmc);


    return p;
}


void make_process_entry(process_t *proc, struct cpu_state *state, void (*address)(void), void *stack)
{
    initialize_cpu_state(proc, state, address, stack, 0);

    if (proc->tls == NULL)
    {
        proc->tls = (struct tls *)stack - 1;

        vmmc_do_lazy_map(proc->vmmc, proc->tls);

        uintptr_t phys;
        kassert_exec_print(vmmc_address_mapped(proc->vmmc, proc->tls, &phys), "proc->tls = %p", proc->tls);

        struct tls *tls = kernel_map(phys, sizeof(*tls));

        tls->absolute = proc->tls;

        tls->errno = 0;
        tls->pid   = proc->pid;
        tls->pgid  = proc->pgid;
        tls->ppid  = proc->ppid;

        kernel_unmap(tls, sizeof(*tls));

        stack = process_stack_alloc(state, sizeof(struct tls));
    }

    if (proc->irq_stack_top == NULL)
        proc->irq_stack_top = stack;
}


void process_create_basic_mappings(process_t *proc)
{
    vmmc_lazy_map_area(proc->vmmc, (void *)USER_STACK_BASE, USER_STACK_TOP - USER_STACK_BASE, VMM_UR | VMM_UW);

    vmmc_lazy_map_area(proc->vmmc, (void *)USER_HERITAGE_BASE, USER_HERITAGE_TOP - USER_HERITAGE_BASE, VMM_UR | VMM_UW);
}


void process_set_args(process_t *proc, struct cpu_state *state, int argc, const char *const *argv, int envc, const char *const *envp)
{
    size_t sz = 0;

    for (int i = 0; i < argc; i++)
        sz += strlen(argv[i]) + 1;

    for (int i = 0; i < envc; i++)
        sz += strlen(envp[i]) + 1;

    sz += sizeof(char *) * (argc + 1) + sizeof(char *) * (envc + 1); // Jeweils Platz für NULL am Ende

    // Schön alignen
    sz = (sz + sizeof(char *) - 1) & ~(sizeof(char *) - 1);


    // FIXME: Dies nimmt einen Descending Stack an.
    void *initial = process_stack_alloc(state, 0);

    void *stack = process_stack_alloc(state, sz);


    uintptr_t end = ((uintptr_t)initial + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (uintptr_t stack_page = (uintptr_t)stack & ~(PAGE_SIZE - 1); stack_page < end; stack_page += PAGE_SIZE)
        vmmc_do_lazy_map(proc->vmmc, (void *)stack_page);


    char *d = (char *)&((char **)stack)[argc + envc + 2];


    uintptr_t paddr;

    kassert_exec(vmmc_address_mapped(proc->vmmc, stack, &paddr));

    void *argv_map = kernel_map(paddr & ~(PAGE_SIZE - 1), PAGE_SIZE);

    for (int i = 0; i < argc + envc + 2; i++)
    {
        uintptr_t argv_i = (uintptr_t)&((char **)stack)[i];

        if (!(argv_i % PAGE_SIZE))
        {
            // Klar kann es passieren, dass das vor dem for gemappt und jetzt
            // gleich wieder geunmappt wird. Aber das ist 1/1024 und ist auch
            // nicht schlimm.
            kernel_unmap(argv_map, PAGE_SIZE);

            kassert_exec(vmmc_address_mapped(proc->vmmc, (void *)argv_i, &paddr));

            argv_map = kernel_map(paddr, PAGE_SIZE);
        }

        if ((i != argc) && (i != argc + 1 + envc))
            ((char **)argv_map)[(argv_i % PAGE_SIZE) / sizeof(char **)] = d;
        else
        {
            ((char **)argv_map)[(argv_i % PAGE_SIZE) / sizeof(char **)] = NULL;
            continue;
        }


        const char *src = (i < argc) ? argv[i] : envp[i - argc - 1];


        size_t len = strlen(src) + 1;
        size_t copied = 0;

        uintptr_t start_p = (uintptr_t)d & ~(PAGE_SIZE - 1);
        uintptr_t end_p   = ((uintptr_t)d + len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        ptrdiff_t out_off = (uintptr_t)d % PAGE_SIZE;

        // Schön pageweise reinkopieren
        for (uintptr_t page = start_p; page < end_p; page += PAGE_SIZE)
        {
            kassert_exec(vmmc_address_mapped(proc->vmmc, (void *)page, &paddr));

            char *f_d = kernel_map(paddr, PAGE_SIZE);

            memcpy(f_d + out_off, src + copied, (page + PAGE_SIZE >= end_p) ? (len - copied) : (size_t)(PAGE_SIZE - out_off));

            copied += PAGE_SIZE - out_off;
            out_off = 0;

            kernel_unmap(f_d, PAGE_SIZE);
        }


        d += len;
    }

    kernel_unmap(argv_map, PAGE_SIZE);


    process_set_initial_params(proc, state, argc, stack, &((const char *const *)stack)[argc + 1]);
}


void make_idle_process(void)
{
    idle_process = kcalloc(sizeof(*idle_process));

    idle_process->status = PROCESS_ACTIVE;

    idle_process->pgid = idle_process->pid = get_new_pid();

    strcpy(idle_process->name, "[idle]");

    idle_process->popup_stack_index = -1;
    idle_process->handled_irq = -1;


    initialize_orphan_process_arch(idle_process);

    alloc_cpu_state(idle_process);


    idle_process->vmmc = &kernel_context;
    use_vmm_context(idle_process->vmmc);
}


process_t *create_kernel_thread(const char *name, void (*function)(void))
{
    process_t *p = kcalloc(sizeof(*p));

    p->status = PROCESS_COMA;

    p->pgid = p->pid = get_new_pid();

    strncpy(p->name, name, sizeof(p->name) - 1);

    p->popup_stack_index = -1;
    p->handled_irq = -1;


    initialize_orphan_process_arch(p);

    alloc_cpu_state(p);


    p->vmmc = &kernel_context;
    use_vmm_context(p->vmmc);


    initialize_kernel_thread_cpu_state(p, p->cpu_state, function);


    return p;
}


process_t *create_user_thread(void (*function)(void *), void *stack_top, void *arg)
{
    process_t *p = kcalloc(sizeof(*p));

    p->status = PROCESS_COMA;

    p->pid = get_new_pid();
    p->ppid = p->pgid = current_process->pgid;

    strncpy(p->name, current_process->name, sizeof(p->name) - 1);

    p->popup_stack_mask = current_process->popup_stack_mask;
    p->popup_stack_index = -1;

    if (p->popup_stack_mask != NULL)
        p->popup_stack_mask->refcount++;

    p->handled_irq = -1;


    initialize_child_process_arch(p, current_process);

    alloc_cpu_state(p);


    p->vmmc = current_process->vmmc;
    use_vmm_context(p->vmmc);


    make_process_entry(p, p->cpu_state, (void (*)(void))function, stack_top);

    add_process_func_param(p, p->cpu_state, (uintptr_t)arg);
    process_simulate_func_call(p->cpu_state);


    return p;
}


void run_process(process_t *proc)
{
    proc->status = PROCESS_ACTIVE;

    register_process(proc);
}


// FIXME: Zwei Versionen sind doof (eine für die Runqueue, eine für alles
// andere).
void rq_register_process(process_t *proc);

void rq_register_process(process_t *proc)
{
    do
        proc->rq_next = runqueue;
    while (!__sync_bool_compare_and_swap(&runqueue, proc->rq_next, proc));
}


void q_register_process(process_t **list, process_t *proc);

// Locking muss der Aufrufer machen
void q_register_process(process_t **list, process_t *proc)
{
    if (list == &runqueue)
        rq_register_process(proc);
    else
        do
            proc->next = *list;
        while (!__sync_bool_compare_and_swap(list, proc->next, proc));
}


void register_process(process_t *proc)
{
    q_register_process(&runqueue, proc);
}


// FIXME
void rq_unregister_process(process_t *proc);

void rq_unregister_process(process_t *proc)
{
    process_t **lp;

    do
        for (lp = &runqueue; (*lp != NULL) && (*lp != proc); lp = (process_t **)&(*lp)->rq_next);
    while (!__sync_bool_compare_and_swap(lp, proc, proc->rq_next) && (*lp != NULL));
}


// Locking muss der Aufrufer machen
void q_unregister_process(process_t **list, process_t *proc);

void q_unregister_process(process_t **list, process_t *proc)
{
    if (list == &runqueue)
    {
        rq_unregister_process(proc);
        return;
    }


    process_t **lp;

    do
        for (lp = list; (*lp != NULL) && (*lp != proc); lp = (process_t **)&(*lp)->next);
    while (!__sync_bool_compare_and_swap(lp, proc, proc->next) && (*lp != NULL));
}


pid_t find_daemon_by_name(const char *name)
{
    process_t *p;
    for (p = daemons; (p != NULL) && strncmp(p->name, name, sizeof(p->name)); p = p->next);

    return (p != NULL) ? p->pid : -1;
}


// FIXME
static process_t *find_process_in_runqueue(pid_t pid)
{
    process_t *p;
    for (p = runqueue; (p != NULL) && (p->pid != pid); p = p->rq_next);

    return p;
}


static process_t *find_process_in(process_t **list, pid_t pid);

static process_t *find_process_in(process_t **list, pid_t pid)
{
    if (list == &runqueue)
        return find_process_in_runqueue(pid);


    process_t *p;
    for (p = *list; (p != NULL) && (p->pid != pid); p = p->next);

    return p;
}


process_t *find_process(pid_t pid)
{
    process_t *p = find_process_in(&runqueue, pid);

    if (p != NULL)
        return p;

    return find_process_in(&daemons, pid);
}


void set_process_popup_entry(process_t *proc, void (*entry)(void))
{
    proc->popup_entry = entry;

    if (proc->popup_stack_mask == NULL)
    {
        proc->popup_stack_mask = kcalloc(sizeof(*proc->popup_stack_mask));
        proc->popup_stack_mask->refcount = 1;
    }
}


void destroy_process_struct(process_t *proc)
{
    if (proc->popup_stack_mask != NULL)
        if (!__sync_sub_and_fetch(&proc->popup_stack_mask->refcount, 1))
            kfree(proc->popup_stack_mask);

    destroy_process_arch_struct(proc);

    unuse_vmm_context(proc->vmmc);

    if (!proc->vmmc->users)
        kfree(proc->vmmc);

    relinquish_pid(proc->pid);

    kfree(proc);
}


static void adopt(pid_t parent_pid, process_t *child)
{
    child->ppid = parent_pid;
    child->tls->ppid = parent_pid;
}

static void adopt_orphans(process_t *parent)
{
    for (process_t *c = runqueue; c != NULL; c = c->rq_next)
        if (c->ppid == parent->pid)
            adopt(parent->ppid, c);

    for (process_t *c = daemons; c != NULL; c = c->next)
        if (c->ppid == parent->pid)
            adopt(parent->ppid, c);

    for (process_t *c = zombies; c != NULL; c = c->next)
        if (c->ppid == parent->pid)
            adopt(parent->ppid, c);
}


void destroy_process(process_t *proc, uintmax_t exit_info)
{
    if (proc->handled_irq >= 0)
    {
        if ((proc->status == PROCESS_ACTIVE) && proc->handling_irq)
            irq_settled(proc->handled_irq);

        unregister_isr_process(proc);
    }


    bool is_current = proc == current_process;


    adopt_orphans(proc);


    if (is_current)
        lock(&schedule_lock);

    q_unregister_process(&runqueue, proc);
    q_register_process(&zombies, proc);

    proc->exit_info = exit_info;
    proc->status = PROCESS_ZOMBIE;

    if (is_current)
    {
        unlock(&schedule_lock);

        for (;;)
            yield();
    }
}


void destroy_this_popup_thread(uintmax_t exit_info)
{
    int psi = current_process->popup_stack_index;

    if ((psi < 0) || (psi >= POPUP_STACKS))
        return;

    kfree(current_process->popup_tmp);

    process_t *pg = find_process(current_process->pgid);

    kassert(pg != NULL);

    pg->popup_stack_mask->mask[psi / LONG_BIT] &= ~(1 << (psi % LONG_BIT));


    // TODO: Userstack freigeben


    if (current_process->popup_zombify)
        destroy_process(current_process, exit_info);
    else
    {
        adopt_orphans(current_process);

        if (current_process->popup_return != NULL)
        {
            kfree(current_process->popup_return->mem);
            kfree(current_process->popup_return);
        }
    }


    lock(&schedule_lock);

    q_unregister_process(&runqueue, current_process);
    q_register_process(&dead, current_process);

    current_process->status = PROCESS_DESTRUCT;

    unlock(&schedule_lock);


    for (;;)
        yield();
}


// FIXME
static process_t *find_child_in_runqueue(pid_t pid);

static process_t *find_child_in_runqueue(pid_t pid)
{
    process_t *p;
    for (p = runqueue; (p != NULL) && (p->ppid != pid); p = p->rq_next);

    return p;
}


static process_t *find_child_in(process_t **list, pid_t pid);

static process_t *find_child_in(process_t **list, pid_t pid)
{
    if (list == &runqueue)
        return find_child_in_runqueue(pid);


    process_t *p;
    for (p = *list; (p != NULL) && (p->ppid != pid); p = p->next);

    return p;
}


static pid_t wait_for_any_child(uintmax_t *status, int options);

static pid_t wait_for_any_child(uintmax_t *status, int options)
{
    process_t *p;

    do
    {
        p = find_child_in(&zombies, current_process->pid);

        if ((p == NULL) && !(options & WNOHANG))
            yield();
    }
    while ((p == NULL) && !(options & WNOHANG));


    if (p == NULL)
        return 0;

    // Ich fauler Sack
    return raw_waitpid(p->pid, status, options, NULL, NULL);
}


pid_t raw_waitpid(pid_t pid, uintmax_t *status, int options, int *errnop, popup_return_memory_t **prmp)
{
    if (pid == -1)
        return wait_for_any_child(status, options);


    volatile process_t *proc = (volatile process_t *)find_process(pid);

    if (proc == NULL)
    {
        proc = (volatile process_t *)find_process_in(&zombies, pid);

        if (proc == NULL)
        {
            current_process->tls->errno = ECHILD;
            return -1;
        }
    }


    if ((proc->status != PROCESS_ZOMBIE) && (options & WNOHANG))
        return 0;


    // FIXME: Das ist alles ziemlich fehleranfällig, sobald mehrere Prozesse auf
    // den hier warten (was aber nicht passieren sollte)
    while (proc->status != PROCESS_ZOMBIE)
        yield_to(proc->pid);

    q_unregister_process(&zombies, (process_t *)proc);

    if (status != NULL)
        *status = proc->exit_info;

    if (errnop != NULL)
    {
        uintptr_t phys;
        kassert_exec_print(vmmc_address_mapped(proc->vmmc, &proc->tls->errno, &phys), "&proc->tls->errno = %p", &proc->tls->errno);

        int *proc_errno = kernel_map(phys, sizeof(int));
        *errnop = *proc_errno;
        kernel_unmap(proc_errno, sizeof(int));
    }

    if (prmp != NULL)
        *prmp = proc->popup_return;

    pid = proc->pid;

    destroy_process_struct((process_t *)proc);

    return pid;
}


void sweep_dead_processes(void)
{
    if (dead == NULL)
        return;


    while (dead != NULL)
    {
        process_t *p = dead;

        if (!__sync_bool_compare_and_swap(&dead, p, p->next))
            continue;

        destroy_process_struct(p);
    }
}


void daemonize_process(process_t *proc, const char *name)
{
    strncpy(proc->name, name, sizeof(proc->name));


    bool is_current = proc == current_process;

    if (is_current)
        lock(&schedule_lock);

    if (proc->handled_irq < 0)
        q_unregister_process(&runqueue, proc);

    q_register_process(&daemons, proc);

    proc->status = PROCESS_DAEMON;

    if (is_current)
    {
        unlock(&schedule_lock);

        for (;;)
            yield();
    }
}


void daemonize_from_irq_handler(process_t *proc)
{
    kassert(proc->handling_irq);
    kassert(proc->handled_irq >= 0);

    proc->handling_irq = false;

    __sync_fetch_and_add(&proc->settle_count, 1);

    // Der Reaper macht den Rest
    proc->status = PROCESS_DAEMON;

    if (proc == current_process)
        for (;;)
            yield();
}


pid_t popup(process_t *proc, int func_index, uintptr_t shmid, const void *buffer, size_t length, bool zombify)
{
    if (proc->popup_stack_mask == NULL)
        return -EINVAL;


    uintptr_t stack_top = 0;
    int stack_index = -1;

    for (int i = 0; i < POPUP_STACKS / LONG_BIT; i++)
    {
        if (proc->popup_stack_mask->mask[i] != ULONG_MAX)
        {
            int bit = ffsl(~proc->popup_stack_mask->mask[i]);

            unsigned long old = proc->popup_stack_mask->mask[i];
            unsigned long new = proc->popup_stack_mask->mask[i] | (1 << (bit - 1));

            if (!__sync_bool_compare_and_swap(&proc->popup_stack_mask->mask[i], old, new))
            {
                i--;
                continue;
            }

            stack_top = USER_STACK_BASE + (i * LONG_BIT + bit) * POPUP_STACK_SIZE;

            stack_index = (i * LONG_BIT) + (bit - 1);

            break;
        }
    }


    if (!stack_top)
        return -EAGAIN;


    // i have no idea (FIXME)
    __asm__ __volatile__ ("cli");


    process_t *pop = kcalloc(sizeof(*pop));

    pop->status = PROCESS_COMA;

    pop->pid  = get_new_pid();
    pop->pgid = proc->pgid;
    pop->ppid = current_process->pid;

    pop->tls = (struct tls *)stack_top - 1;
    stack_top = (uintptr_t)pop->tls;


    vmmc_do_lazy_map(proc->vmmc, pop->tls);

    uintptr_t phys;
    kassert_exec_print(vmmc_address_mapped(proc->vmmc, pop->tls, &phys), "pop->tls = %p", pop->tls);

    struct tls *tls = kernel_map(phys, sizeof(*tls));

    tls->absolute = pop->tls;

    tls->errno = 0;
    tls->pid   = pop->pid;
    tls->pgid  = pop->pgid;
    tls->ppid  = pop->ppid;

    kernel_unmap(tls, sizeof(*tls));

    strcpy(pop->name, "[popup]");

    pop->popup_stack_index = stack_index;
    pop->handled_irq = -1;


    initialize_child_process_arch(pop, proc);

    alloc_cpu_state(pop);

    if (buffer == NULL)
        pop->popup_tmp = NULL;
    else
    {
        pop->popup_tmp = kmalloc(length);
        pop->popup_tmp_sz = length;

        memcpy(pop->popup_tmp, buffer, length);
    }


    pop->popup_zombify = zombify;


    pop->vmmc = proc->vmmc;

    use_vmm_context(pop->vmmc);


    initialize_cpu_state(pop, pop->cpu_state, proc->popup_entry, (void *)stack_top, 2, func_index, shmid);


    register_process(pop);


    pop->status = PROCESS_ACTIVE;


    return pop->pid;
}


pid_t fork(struct cpu_state *current_state)
{
    process_t *child = create_empty_process(current_process->name);

    if (current_process->popup_entry != NULL)
        set_process_popup_entry(child, current_process->popup_entry);

    child->tls = current_process->tls;
    child->irq_stack_top = current_process->irq_stack_top;


    vmmc_clone(child->vmmc, current_process->vmmc);


    initialize_child_process_arch(child, current_process);

    initialize_forked_cpu_state(child->cpu_state, current_state);


    vmmc_do_lazy_map(child->vmmc, child->tls);

    uintptr_t phys;
    kassert_exec(vmmc_address_mapped(child->vmmc, child->tls, &phys));

    struct tls *tls = kernel_map(phys, sizeof(*tls));

    tls->pid  = child->pid;
    tls->pgid = child->pgid;
    tls->ppid = child->ppid;

    kernel_unmap(tls, sizeof(*tls));


    register_process(child);


    child->status = PROCESS_ACTIVE;


    return child->pid;
}


int exec(struct cpu_state *state, const void *file, size_t file_length, char *const *argv, char *const *envp)
{
    if (!check_process_file_image(file))
    {
        current_process->tls->errno = ENOEXEC;
        return -1;
    }


    void *mem = kmalloc(file_length);
    memcpy(mem, file, file_length);

    int argc, envc;
    for (argc = 0; argv[argc] != NULL; argc++);
    for (envc = 0; envp[envc] != NULL; envc++);

    char *kargv[argc], *kenvp[envc];

    for (int i = 0; argv[i] != NULL; i++)
        kargv[i] = strdup(argv[i]);

    for (int i = 0; envp[i] != NULL; i++)
        kenvp[i] = strdup(envp[i]);


    strncpy(current_process->name, kargv[0], sizeof(current_process->name) - 1);
    current_process->name[sizeof(current_process->name) - 1] = 0;


    vmmc_clear_user(current_process->vmmc, true);

    vmmc_lazy_map_area(current_process->vmmc, (void *)USER_STACK_BASE, USER_STACK_TOP - USER_STACK_BASE, VMM_UR | VMM_UW);


    void *heap_start, (*entry)(void);
    kassert_exec(load_image_to_process(current_process, mem, &heap_start, &entry));


    vmmc_set_heap_top(current_process->vmmc, heap_start);

    current_process->tls = NULL;
    current_process->irq_stack_top = NULL;

    make_process_entry(current_process, state, entry, (void *)USER_STACK_TOP);

    process_set_args(current_process, state, argc, (const char **)kargv, envc, (const char **)kenvp);


    kfree(mem);

    for (int i = 0; i < argc; i++)
        kfree(kargv[i]);

    for (int i = 0; i < envc; i++)
        kfree(kenvp[i]);


    return 0;
}


bool is_popup(process_t *proc)
{
    return proc->popup_stack_index >= 0;
}
