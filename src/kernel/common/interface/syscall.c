#include <arch-constants.h>
#include <errno.h>
#include <process.h>
#include <stdint.h>
#include <syscall.h>


uintptr_t syscall5(int syscall_nr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4)
{
    switch (syscall_nr)
    {
        case SYS_EXIT:
            destroy_process(current_process);
            return 0;

        case SYS_SET_ERRNO:
            if (IS_KERNEL(p0))
            {
                // errno = EFAULT wäre wohl ziemlich doof
                return 0;
            }
            current_process->errno = (int *)p0;
            return 1;


        case SYS_MAP_MEMORY:
            // TODO: Checks, natürlich
            return (uintptr_t)vmmc_user_map(current_process->vmmc, p0, p1, p2);

        case SYS_DAEMONIZE:
            daemonize_process(current_process);
            return 0;

        case SYS_POPUP_ENTRY:
            if (IS_KERNEL(p0))
            {
                *current_process->errno = EFAULT;
                return 0;
            }
            set_process_popup_entry(current_process, (void (*)(void))p0);
            return 0;

        case SYS_POPUP_EXIT:
            destroy_this_popup_thread();
            *current_process->errno = EINVAL;
            return 0;

        case SYS_IPC_MESSAGE:
        {
            process_t *proc = find_process(p0);
            if (proc == NULL)
            {
                *current_process->errno = ESRCH;
                return 0;
            }
            int ret = popup(proc);
            if (ret < 0)
                *current_process->errno = -ret;
            return 0;
        }
    }

    *current_process->errno = ENOSYS;
    return 0;
}
