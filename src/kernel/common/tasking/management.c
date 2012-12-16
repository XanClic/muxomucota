#include <arch-constants.h>
#include <cpu.h>
#include <cpu-state.h>
#include <errno.h>
#include <kassert.h>
#include <kmalloc.h>
#include <limits.h>
#include <lock.h>
#include <process.h>
#include <stdint.h>
#include <string.h>
#include <vmem.h>
#include <sys/types.h>


lock_t runqueue_lock = locked;


process_t *current_process, *idle_process, *runqueue;

static pid_t pid_counter = 0;


void start_doing_stuff(void)
{
    runqueue_lock = unlocked;
}


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
    initialize_cpu_state(proc->cpu_state, address, stack);

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

    idle_process->runqueue_next = idle_process;


    alloc_cpu_state(idle_process);


    idle_process->vmmc = &kernel_context;
    use_vmm_context(idle_process->vmmc);
}


void register_process(process_t *proc)
{
    // FIXME: Locking

    if (runqueue != NULL)
    {
        proc->runqueue_next = runqueue->runqueue_next;
        runqueue->runqueue_next = proc;
    }
    else
    {
        proc->runqueue_next = proc;
        runqueue = proc;
    }
}


process_t *find_process(pid_t pid)
{
    process_t *initial = runqueue, *rq = initial;

    if (rq->pid != pid)
        do
            rq = rq->runqueue_next;
        while ((rq->pid != pid) && (rq != initial));

    if (rq == initial)
        return NULL;

    return rq;
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
    proc->status = PROCESS_ZOMBIE;

    if (proc == current_process)
        for (;;)
            cpu_halt();
}


void destroy_this_popup_thread(void)
{
    int psi = current_process->popup_stack_index;

    if ((psi < 0) || (psi >= POPUP_STACKS))
        return;

    process_t *pg = find_process(current_process->pgid);

    kassert(pg != NULL);

    pg->popup_stack_mask[psi / LONG_BIT] &= ~(1 << (psi % LONG_BIT));

    current_process->status = PROCESS_DESTRUCT;

    for (;;)
        cpu_halt();
}


void daemonize_process(process_t *proc)
{
    // TODO: Aus der Runqueue nehmen

    proc->status = PROCESS_DAEMON;

    if (proc == current_process)
        for (;;)
            cpu_halt();
}


int popup(process_t *proc)
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

    pop->name[0] = 0;

    pop->popup_stack_mask = NULL;
    pop->popup_stack_index = stack_index;


    alloc_cpu_state(pop);


    pop->vmmc = proc->vmmc;

    use_vmm_context(pop->vmmc);


    initialize_cpu_state(pop->cpu_state, proc->popup_entry, (void *)stack_top);


    // FIXME: Atomic
    pop->runqueue_next = proc->runqueue_next;
    proc->runqueue_next = pop;


    pop->status = PROCESS_ACTIVE;


    return 0;
}
