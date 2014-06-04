#include <ipc.hpp>
#include <kassert.hpp>
#include <lock.hpp>
#include <process.hpp>


mu::lock::lock(void):
    holder(0)
{
}


void mu::lock::init(void)
{
    holder = 0;
}


void mu::lock::acquire(void)
{
    pid_t mypid = current_process ? current_process->pid : -1, owner;

    while ((owner = __sync_val_compare_and_swap(&holder, 0, mypid)) != 0) {
        kassert(mypid != owner);
        mu::yield_to(owner);
    }
}


bool mu::lock::acquire_nonblock(void)
{
    return __sync_bool_compare_and_swap(&holder, 0, current_process ? current_process->pid : -1);
}


void mu::lock::release(void)
{
    __sync_lock_release(&holder);
}


bool mu::lock::taken(void) const
{
    return holder;
}
