#include <arch-constants.h>
#include <errno.h>
#include <ipc.h>
#include <process.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>
#include <vmem.h>


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
            if (IS_KERNEL(p0))
                p0 = 0;
            daemonize_process(current_process, (const char *)p0);
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
            current_process->exit_info = p0;
            destroy_this_popup_thread();
            *current_process->errno = EINVAL;
            return 0;

        case SYS_IPC_MESSAGE:
        {
            if (IS_KERNEL(p2))
            {
                *current_process->errno = EFAULT;
                return 0;
            }
            process_t *proc = find_process(p0);
            if (proc == NULL)
            {
                *current_process->errno = ESRCH;
                return 0;
            }
            pid_t ret = popup(proc, p1, (const void *)p2, p3, p4);
            if (ret < 0)
                *current_process->errno = -ret;
            else if (p4)
                return raw_waitpid(ret);
            return 0;
        }

        case SYS_POPUP_GET_MESSAGE:
            if (IS_KERNEL(p0))
                *current_process->errno = EFAULT;
            else if (p0)
                memcpy((void *)p0, current_process->popup_tmp, (current_process->popup_tmp_sz > p1) ? p1 : current_process->popup_tmp_sz);
            return current_process->popup_tmp_sz;

        case SYS_FIND_DAEMON_BY_NAME:
            if (IS_KERNEL(p0))
            {
                *current_process->errno = EFAULT;
                return -1;
            }
            return find_daemon_by_name((const char *)p0);

        case SYS_SHM_CREATE:
            return vmmc_create_shm(p0);

        case SYS_SHM_OPEN:
            if (!IS_KERNEL(p0))
            {
                *current_process->errno = EINVAL;
                return 0;
            }
            // FIXME: p0 (shm_id) noch besser testen
            return (uintptr_t)vmmc_open_shm(current_process->vmmc, p0, p1);

        case SYS_SHM_CLOSE:
            if (!IS_KERNEL(p0))
            {
                *current_process->errno = EINVAL;
                return 0;
            }
            else if (IS_KERNEL(p1))
            {
                *current_process->errno = EFAULT;
                return 0;
            }
            // FIXME: p0 (shm_id) noch besser testen; außerdem p1 testen
            // (oder besser: gar nicht vom Benutzer nehmen)
            vmmc_close_shm(current_process->vmmc, p0, (void *)p1);
            return 0;

        case SYS_SHM_SIZE:
            if (!IS_KERNEL(p0))
            {
                *current_process->errno = EINVAL;
                return 0;
            }
            // FIXME: p0 (shm_id) noch besser testen
            return vmmc_get_shm_size(p0);

        case SYS_IPC_SHM:
        {
            if (!IS_KERNEL(p2))
            {
                *current_process->errno = EINVAL;
                return 0;
            }
            process_t *proc = find_process(p0);
            if (proc == NULL)
            {
                *current_process->errno = ESRCH;
                return 0;
            }
            // FIXME: p2 (shm_id) noch besser testen
            pid_t ret = popup_shm(proc, p1, p2, p3);
            if (ret < 0)
                *current_process->errno = -ret;
            else if (p3)
                return raw_waitpid(ret);
            return 0;
        }

        case SYS_SBRK:
            return (uintptr_t)context_sbrk(current_process->vmmc, (ptrdiff_t)p0);
    }

    *current_process->errno = ENOSYS;
    return 0;
}
