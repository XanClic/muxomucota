#include <arch-constants.h>
#include <cpu-state.h>
#include <errno.h>
#include <ipc.h>
#include <kassert.h>
#include <kmalloc.h>
#include <limits.h>
#include <lock.h>
#include <process.h>
#include <stdint.h>
#include <string.h>
#include <vmem.h>
#include <sys/types.h>


// FIXME: Die Ringlisten sind doof.


lock_t runqueue_lock = unlocked, daemons_lock = unlocked, zombies_lock = unlocked, dead_lock = unlocked; // haha dead_lock


process_t *current_process, *idle_process, *runqueue, *daemons, *zombies, *dead;

static pid_t pid_counter = 0;


process_t *create_empty_process(const char *name)
{
    process_t *p = kmalloc(sizeof(*p));

    p->status = PROCESS_COMA;

    p->pgid = p->pid = pid_counter++; // FIXME: Atomic

    strncpy(p->name, name, sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = 0;

    p->popup_stack_mask = NULL;
    p->popup_stack_index = -1;


    alloc_cpu_state(p);


    p->vmmc = kmalloc(sizeof(*p->vmmc));

    create_vmm_context(p->vmmc);
    use_vmm_context(p->vmmc);


    return p;
}


void make_process_entry(process_t *proc, void (*address)(void), void *stack)
{
    initialize_cpu_state(proc->cpu_state, address, stack, 0);

    proc->status = PROCESS_ACTIVE;
}


void make_idle_process(void)
{
    idle_process = kmalloc(sizeof(*idle_process));

    idle_process->status = PROCESS_ACTIVE;

    idle_process->pgid = idle_process->pid = pid_counter++; // FIXME: Atomic

    strcpy(idle_process->name, "[idle]");

    idle_process->popup_stack_mask = NULL;
    idle_process->popup_stack_index = -1;

    idle_process->next = idle_process;


    alloc_cpu_state(idle_process);


    idle_process->vmmc = &kernel_context;
    use_vmm_context(idle_process->vmmc);
}


void q_register_process(process_t **list, process_t *proc);

// Locking muss der Aufrufer machen
void q_register_process(process_t **list, process_t *proc)
{
    if (*list != NULL)
    {
        proc->next = (*list)->next;
        (*list)->next = proc;
    }
    else
    {
        proc->next = proc;
        *list = proc;
    }
}


void register_process(process_t *proc)
{
    kassert_exec(lock(&runqueue_lock));

    q_register_process(&runqueue, proc);

    unlock(&runqueue_lock);
}


// Locking muss der Aufrufer machen
void q_unregister_process(process_t **list, process_t *proc);

void q_unregister_process(process_t **list, process_t *proc)
{
    process_t **lp = list;

    // FIXME: Prozess nicht in der Liste

    if (proc == proc->next)
    {
        *list = NULL;
        return;
    }

    // Weil Ring
    lp = &(*lp)->next;

    while (*lp != proc)
        lp = &(*lp)->next;

    if (*list == proc)
        *list = proc->next;

    *lp = proc->next;
}


pid_t find_daemon_by_name(const char *name)
{
    kassert_exec(lock(&daemons_lock));

    process_t *p = daemons;

    if (!strncmp(p->name, name, sizeof(p->name)))
    {
        unlock(&daemons_lock);
        return p->pid;
    }

    do
        p = p->next;
    while (strncmp(p->name, name, sizeof(p->name)) && (p != daemons));

    unlock(&daemons_lock);

    return (p != NULL) ? p->pid : -1;
}


static process_t *find_process_in(process_t **list, pid_t pid);

static process_t *find_process_in(process_t **list, pid_t pid)
{
    process_t *p = *list;

    if (p->pid == pid)
        return p;

    do
        p = p->next;
    while ((p->pid != pid) && (p != *list));

    if (p->pid == pid)
        return p;

    return NULL;
}


process_t *find_process(pid_t pid)
{
    process_t *p;

    kassert_exec(lock(&runqueue_lock));
    p = find_process_in(&runqueue, pid);
    unlock(&runqueue_lock);

    if (p != NULL)
        return p;

    kassert_exec(lock(&daemons_lock));
    p = find_process_in(&daemons,  pid);
    unlock(&daemons_lock);

    return p;
}


void set_process_popup_entry(process_t *proc, void (*entry)(void))
{
    proc->popup_entry = entry;

    proc->popup_stack_mask = kmalloc(POPUP_STACKS / 8);

    memset(proc->popup_stack_mask, 0, POPUP_STACKS / 8);
}


void destroy_process_struct(process_t *proc)
{
    kfree(proc->popup_stack_mask);

    destroy_process_arch_struct(proc);

    unuse_vmm_context(proc->vmmc);

    if (!proc->vmmc->users)
        kfree(proc->vmmc);

    kfree(proc);
}


void destroy_process(process_t *proc)
{
    kassert_exec(lock(&runqueue_lock));

    q_unregister_process(&runqueue, proc);

    // yay potentieller dead lock (FIXME, btw)
    kassert_exec(lock(&zombies_lock));

    q_register_process(&zombies, proc);

    unlock(&zombies_lock);

    proc->status = PROCESS_ZOMBIE;

    unlock(&runqueue_lock);


    if (proc == current_process)
        for (;;)
            yield();
}


void destroy_this_popup_thread(void)
{
    int psi = current_process->popup_stack_index;

    if ((psi < 0) || (psi >= POPUP_STACKS))
        return;

    kfree(current_process->popup_tmp);

    process_t *pg = find_process(current_process->pgid);

    kassert(pg != NULL);

    pg->popup_stack_mask[psi / LONG_BIT] &= ~(1 << (psi % LONG_BIT));


    // TODO: Userstack freigeben


    if (current_process->popup_zombify)
        destroy_process(current_process);


    kassert_exec(lock(&runqueue_lock));

    q_unregister_process(&runqueue, current_process);

    // yay potentieller dead lock (FIXME, btw)
    kassert_exec(lock(&dead_lock));

    q_register_process(&dead, current_process);

    unlock(&dead_lock);

    current_process->status = PROCESS_DESTRUCT;

    unlock(&runqueue_lock);


    for (;;)
        yield();
}


uintptr_t raw_waitpid(pid_t pid)
{
    volatile process_t *proc = (volatile process_t *)find_process(pid);

    if (proc == NULL)
    {
        kassert_exec(lock(&zombies_lock));
        proc = (volatile process_t *)find_process_in(&zombies, pid);
        unlock(&zombies_lock);

        // FIXME: Fehlermitteilung
        if (proc == NULL)
            return 0;
    }


    // FIXME: Das ist alles ziemlich fehleranfällig
    while (proc->status != PROCESS_ZOMBIE)
        yield();

    kassert_exec(lock(&zombies_lock));
    q_unregister_process(&zombies, (process_t *)proc);
    unlock(&zombies_lock);

    uintptr_t exit_val = proc->exit_info;

    destroy_process_struct((process_t *)proc);

    return exit_val;
}


void sweep_dead_processes(void)
{
    if (dead == NULL)
        return;

    process_t *p = dead;

    do
    {
        process_t *np = p->next;

        destroy_process_struct(p);

        p = np;
    }
    while (p != dead);

    dead = NULL;
}


void daemonize_process(process_t *proc, const char *name)
{
    strncpy(proc->name, name, sizeof(proc->name));

    kassert_exec(lock(&runqueue_lock));

    q_unregister_process(&runqueue, proc);

    // yay potentieller dead lock (FIXME, btw)
    kassert_exec(lock(&daemons_lock));

    q_register_process(&daemons, proc);

    unlock(&daemons_lock);

    proc->status = PROCESS_DAEMON;

    unlock(&runqueue_lock);

    if (proc == current_process)
        for (;;)
            yield();
}


pid_t popup(process_t *proc, int func_index, const void *buffer, size_t length, bool zombify)
{
    if (proc->popup_stack_mask == NULL)
        return -EINVAL;


    // TODO
    if (proc->pid != proc->pgid)
        return -EINVAL;


    uintptr_t stack_top = 0;
    int stack_index = -1;

    for (int i = 0; i < POPUP_STACKS; i++)
    {
        if (proc->popup_stack_mask[i] != ULONG_MAX)
        {
            int bit = ffsl(~proc->popup_stack_mask[i]);

            // FIXME: Atomic
            stack_top = USER_STACK_BASE + bit * POPUP_STACK_SIZE;
            proc->popup_stack_mask[i] |= 1 << (bit - 1);

            stack_index = (i * LONG_BIT) + (bit - 1);

            break;
        }
    }


    if (!stack_top)
        return -EAGAIN;


    process_t *pop = kmalloc(sizeof(*pop));

    pop->status = PROCESS_COMA;

    pop->pid = pid_counter++; // FIXME: Atomic
    pop->pgid = proc->pid;

    pop->errno = proc->errno;

    pop->name[0] = 0;

    pop->popup_stack_mask = NULL;
    pop->popup_stack_index = stack_index;


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


    initialize_cpu_state(pop->cpu_state, proc->popup_entry, (void *)stack_top, 1, func_index);


    register_process(pop);


    pop->status = PROCESS_ACTIVE;


    return pop->pid;
}
