#include <process.hpp>


process *current_process, *idle_process, *runqueue, *daemons, *zombies, *dead;


static process *find_process_in_runqueue(pid_t pid)
{
    process *p;
    for (p = runqueue; p && (p->pid != pid); p = p->rq_next);

    return p;
}


static process *find_process_in(process **list, pid_t pid)
{
    if (list == &runqueue) {
        return find_process_in_runqueue(pid);
    }

    process *p;
    for (p = *list; p && (p->pid != pid); p = p->next);

    return p;
}


process *process::find(pid_t pid)
{
    process *p = find_process_in(&runqueue, pid);

    if (p) {
        return p;
    }

    return find_process_in(&daemons, pid);
}
