#include <arch-constants.h>
#include <cpu-state.h>
#include <errno.h>
#include <ipc.h>
#include <isr.h>
#include <process.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>
#include <vmem.h>


// FIXME: Überprüfung, ob der Anfang der übergebenen Zeiger im Kernel liegt,
// ist gut. Aber überprüfen, ob stattdessen das Ende drin liegt, wäre noch
// besser.


uintptr_t syscall_krn(int syscall_nr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4, struct cpu_state *state)
{
    (void)p4;


    switch (syscall_nr)
    {
        case SYS_EXIT:
            destroy_process(current_process, p0);
            return 0;

        case SYS_MAP_MEMORY:
            // TODO: Checks, natürlich
            return (uintptr_t)vmmc_user_map(current_process->vmmc, p0, p1, p2);

        case SYS_UNMAP_MEMORY:
            if (IS_KERNEL(p0))
            {
                current_process->tls->errno = EFAULT;
                return 0;
            }
            vmmc_user_unmap(current_process->vmmc, (void *)p0, p1);
            return 0;

        case SYS_DAEMONIZE:
            if (IS_KERNEL(p0))
                p0 = 0;
            daemonize_process(current_process, (const char *)p0);
            return 0;

        case SYS_POPUP_ENTRY:
            if (IS_KERNEL(p0))
            {
                current_process->tls->errno = EFAULT;
                return 0;
            }
            set_process_popup_entry(current_process, (void (*)(void))p0);
            return 0;

        case SYS_POPUP_EXIT:
            if (IS_KERNEL(p0))
            {
                current_process->tls->errno = EFAULT;
                return 0;
            }
            destroy_this_popup_thread(*(uintmax_t *)p0);
            current_process->tls->errno = EINVAL;
            return 0;

        case SYS_IPC_POPUP:
        {
            if (!p0 || IS_KERNEL(p0))
            {
                current_process->tls->errno = EFAULT;
                return 0;
            }

            struct ipc_syscall_params *ipc_sp = (struct ipc_syscall_params *)p0;

            if (ipc_sp->shmid && !IS_KERNEL(ipc_sp->shmid))
            {
                current_process->tls->errno = EINVAL;
                return 0;
            }
            if ((ipc_sp->short_message != NULL) && IS_KERNEL(ipc_sp->short_message))
            {
                current_process->tls->errno = EFAULT;
                return 0;
            }
            if ((ipc_sp->synced_result != NULL) && IS_KERNEL(ipc_sp->synced_result))
            {
                current_process->tls->errno = EFAULT;
                return 0;
            }

            process_t *proc = find_process(ipc_sp->target_pid);
            if (proc == NULL)
            {
                current_process->tls->errno = ESRCH;
                return 0;
            }

            // FIXME: shmid noch besser testen
            pid_t ret = popup(proc, ipc_sp->func_index, ipc_sp->shmid, ipc_sp->short_message, ipc_sp->short_message_length, ipc_sp->synced_result != NULL);
            if (ret < 0)
                current_process->tls->errno = -ret;
            else if (ipc_sp->synced_result != NULL)
                raw_waitpid(ret, ipc_sp->synced_result, 0);
            return 0;
        }

        case SYS_POPUP_GET_MESSAGE:
            if (IS_KERNEL(p0))
                current_process->tls->errno = EFAULT;
            else if (p0)
                memcpy((void *)p0, current_process->popup_tmp, (current_process->popup_tmp_sz > p1) ? p1 : current_process->popup_tmp_sz);
            return current_process->popup_tmp_sz;

        case SYS_FIND_DAEMON_BY_NAME:
            if (IS_KERNEL(p0))
            {
                current_process->tls->errno = EFAULT;
                return -1;
            }
            return find_daemon_by_name((const char *)p0);

        case SYS_SHM_CREATE:
            return vmmc_create_shm(p0);

        case SYS_SHM_MAKE:
            if (IS_KERNEL(p1) || IS_KERNEL(p2))
            {
                current_process->tls->errno = EFAULT;
                return -1;
            }
            return vmmc_make_shm(current_process->vmmc, p0, (void **)p1, (int *)p2, p3);

        case SYS_SHM_OPEN:
            if (!IS_KERNEL(p0))
            {
                current_process->tls->errno = EINVAL;
                return 0;
            }
            // FIXME: p0 (shm_id) noch besser testen
            return (uintptr_t)vmmc_open_shm(current_process->vmmc, p0, p1);

        case SYS_SHM_CLOSE:
            if (!IS_KERNEL(p0))
            {
                current_process->tls->errno = EINVAL;
                return 0;
            }
            else if (IS_KERNEL(p1))
            {
                current_process->tls->errno = EFAULT;
                return 0;
            }
            // FIXME: p0 (shm_id) noch besser testen; außerdem p1 testen
            // (oder besser: gar nicht vom Benutzer nehmen)
            vmmc_close_shm(current_process->vmmc, p0, (void *)p1);
            return 0;

        case SYS_SHM_UNMAKE:
            if (!IS_KERNEL(p0))
            {
                current_process->tls->errno = EINVAL;
                return 0;
            }
            // FIXME: p0 (shm_id) noch besser testen
            vmmc_unmake_shm(p0);
            return 0;

        case SYS_SHM_SIZE:
            if (!IS_KERNEL(p0))
            {
                current_process->tls->errno = EINVAL;
                return 0;
            }
            // FIXME: p0 (shm_id) noch besser testen
            return vmmc_get_shm_size(p0);

        case SYS_SBRK:
            return (uintptr_t)context_sbrk(current_process->vmmc, (ptrdiff_t)p0);

        case SYS_YIELD:
            yield_to(p0);
            return 0;

        case SYS_FORK:
            return fork(state);

        case SYS_EXEC:
            if (IS_KERNEL(p0) || IS_KERNEL(p2) || IS_KERNEL(p3))
            {
                current_process->tls->errno = EFAULT;
                return (uintptr_t)-EFAULT;
            }
            return exec(state, (const void *)p0, p1, (char *const *)p2, (char *const *)p3);

        case SYS_WAIT:
            return raw_waitpid(p0, (uintmax_t *)p1, p2);

        case SYS_HANDLE_IRQ:
            if (current_process->popup_stack_index >= 0)
            {
                // Popupthreads dürfen sich nicht registrieren
                current_process->tls->errno = EPERM;
                return (uintptr_t)-EPERM;
            }
            if (IS_KERNEL(p1))
            {
                current_process->tls->errno = EFAULT;
                return (uintptr_t)-EFAULT;
            }
            register_user_isr(p0, current_process, (void (*)(void *))p1, (void *)p2);
            return 0;

        case SYS_IRQ_HANDLER_EXIT:
            daemonize_from_irq_handler(current_process);
            return 0;

        case SYS_IOPL:
#ifdef X86
            if (p0 > 3)
            {
                current_process->tls->errno = EINVAL;
                return (uintptr_t)-EINVAL;
            }
            kiopl(p0, state);
            return 0;
#else
            current_process->tls->errno = ENOSYS;
            return (uintptr_t)-ENOSYS;
#endif
    }

    current_process->tls->errno = ENOSYS;
    return 0;
}
