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
#include <pmm.h>
#include <process.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>
#include <system-timer.h>
#include <unistd.h>
#include <vmem.h>
#include <vm86.h>


extern volatile int elapsed_ms;


static bool is_valid_user_string(const char *s)
{
    uintptr_t addr = (uintptr_t)s;

    for (;;)
    {
        ptrdiff_t page_data_size = PAGE_SIZE - (addr & (PAGE_SIZE - 1));

        if (!is_valid_user_mem(current_process->vmmc, (const void *)addr, page_data_size, VMM_UR | VMM_PR))
            return false;

        for (int i = 0; i < page_data_size; i++)
            if (!((const char *)addr)[i])
                return true;

        addr += page_data_size;
    }
}


uintptr_t syscall_krn(int syscall_nr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4, struct cpu_state *state)
{
    (void)p4;


    switch (syscall_nr)
    {
        case SYS_EXIT:
            if (is_popup(current_process))
                destroy_this_popup_thread(0);
            else
                destroy_process(current_process, p0);
            return 0;

        case SYS_MAP_MEMORY:
            // TODO: Checks, natürlich
            return (uintptr_t)vmmc_user_map(current_process->vmmc, p0, p1, p2);

        case SYS_UNMAP_MEMORY:
            if (!is_valid_user_mem(current_process->vmmc, (const void *)p0, p1, VMM_UR | VMM_UW))
            {
                current_process->tls->errno = EFAULT;
                return 0;
            }

            vmmc_user_unmap(current_process->vmmc, (void *)p0, p1);
            return 0;

        case SYS_DAEMONIZE:
            kassert(!is_popup(current_process));

            if (!is_valid_user_string((const char *)p0))
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
            if (!is_popup(current_process))
            {
                current_process->tls->errno = EPERM;
                return 0;
            }

            if (!is_valid_user_mem(current_process->vmmc, (const void *)p0, sizeof(uintmax_t), VMM_UR | VMM_UW | VMM_PW))
            {
                current_process->tls->errno = EFAULT;
                return 0;
            }

            destroy_this_popup_thread(*(uintmax_t *)p0);
            current_process->tls->errno = EINVAL;
            return 0;

        case SYS_IPC_POPUP:
        {
            if (!is_valid_user_mem(current_process->vmmc, (const void *)p0, sizeof(struct ipc_syscall_params), VMM_UR | VMM_PR))
            {
                current_process->tls->errno = EFAULT;
                return 0;
            }

            struct ipc_syscall_params *ipc_sp = (struct ipc_syscall_params *)p0;

            if (ipc_sp->shmid && !is_valid_shm(ipc_sp->shmid))
            {
                current_process->tls->errno = EINVAL;
                return 0;
            }
            if ((ipc_sp->short_message != NULL) && !is_valid_user_mem(current_process->vmmc, ipc_sp->short_message, ipc_sp->short_message_length, VMM_UR | VMM_PR))
            {
                current_process->tls->errno = EFAULT;
                return 0;
            }
            if ((ipc_sp->synced_result != NULL) && !is_valid_user_mem(current_process->vmmc, ipc_sp->synced_result, sizeof(*ipc_sp->synced_result), VMM_UR | VMM_PW))
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

            pid_t ret = popup(proc, ipc_sp->func_index, ipc_sp->shmid, ipc_sp->short_message, ipc_sp->short_message_length, ipc_sp->synced_result != NULL);
            if (ret < 0)
            {
                current_process->tls->errno = -ret;
                return 0;
            }
            else if (ipc_sp->synced_result != NULL)
            {
                int errno = 0;
                popup_return_memory_t *prm = NULL;

                raw_waitpid(ret, ipc_sp->synced_result, 0, &errno, &prm);

                if (errno)
                    current_process->tls->errno = errno;

                // FIXME: Das gibt einen Memleak, wenn SYS_POPUP_GET_ANSWER nicht aufgerufen wird
                return (uintptr_t)prm;
            }

            return 0;
        }

        case SYS_POPUP_GET_MESSAGE:
            if (!is_popup(current_process))
            {
                current_process->tls->errno = EPERM;
                return 0;
            }

            if (p0 && p1 && !is_valid_user_mem(current_process->vmmc, (const void *)p0, p1, VMM_UR | VMM_PW))
                current_process->tls->errno = EFAULT;
            else if (p0 && p1)
                memcpy((void *)p0, current_process->popup_tmp, (current_process->popup_tmp_sz > p1) ? p1 : current_process->popup_tmp_sz);
            return current_process->popup_tmp_sz;

        case SYS_FIND_DAEMON_BY_NAME:
            if (!is_valid_user_string((const char *)p0))
            {
                current_process->tls->errno = EFAULT;
                return -1;
            }

            return find_daemon_by_name((const char *)p0);

        case SYS_SHM_CREATE:
            return vmmc_create_shm(p0);

        case SYS_SHM_MAKE:
        {
            void **vaddr_list = (void **)p1;
            int *page_count_list = (int *)p2;

            if (!is_valid_user_mem(current_process->vmmc, vaddr_list, sizeof(vaddr_list[0]) * p0, VMM_UR | VMM_PR) ||
                !is_valid_user_mem(current_process->vmmc, page_count_list, sizeof(page_count_list[0]) * p0, VMM_UR | VMM_PR))
            {
                current_process->tls->errno = EFAULT;
                return 0;
            }

            int count = (int)p0;
            ptrdiff_t offset = (ptrdiff_t)p3;

            if ((count <= 0) || (offset < 0) || (offset >= PAGE_SIZE))
            {
                current_process->tls->errno = EINVAL;
                return 0;
            }

            /*
            // FIXME: Sowas wär schon schön, müsste aber funktionieren
            for (int i = 0; i < count; i++)
            {
                // FIXME: Eigentlich braucht der Benutzer nicht unbedingt VMM_UR-Zugriff, sondern nur irgendeinen Zugriff
                if (!is_valid_user_mem(current_process->vmmc, (void *)((uintptr_t)vaddr_list[i] & ~(PAGE_SIZE - 1)), PAGE_SIZE, VMM_UR))
                {
                    current_process->tls->errno = EFAULT;
                    return 0;
                }
            }
            */

            return vmmc_make_shm(current_process->vmmc, count, vaddr_list, page_count_list, offset);
        }

        case SYS_SHM_OPEN:
            if (!is_valid_shm(p0))
            {
                current_process->tls->errno = EINVAL;
                return 0;
            }

            return (uintptr_t)vmmc_open_shm(current_process->vmmc, p0, p1);

        case SYS_SHM_CLOSE:
        {
            if (!is_valid_shm(p0))
            {
                current_process->tls->errno = EINVAL;
                return 0;
            }

            struct shm_sg *sg = (struct shm_sg *)p0;

            // FIXME: Eigentlich braucht der Benutzer nicht unbedingt VMM_UR-Zugriff, sondern nur irgendeinen Zugriff
            if (!is_valid_user_mem(current_process->vmmc, (const void *)(p1 - sg->offset), sg->size, VMM_UR))
            {
                current_process->tls->errno = EFAULT;
                return 0;
            }

            vmmc_close_shm(current_process->vmmc, p0, (void *)p1, p2);
            return 0;
        }

        case SYS_SHM_UNMAKE:
            if (!is_valid_shm(p0))
            {
                current_process->tls->errno = EINVAL;
                return 0;
            }

            vmmc_unmake_shm(p0, p1);
            return 0;

        case SYS_SHM_SIZE:
            if (!is_valid_shm(p0))
            {
                current_process->tls->errno = EINVAL;
                return 0;
            }

            return vmmc_get_shm_size(p0);

        case SYS_SBRK:
            return (uintptr_t)context_sbrk(current_process->vmmc, (ptrdiff_t)p0);

        case SYS_YIELD:
            yield_to(p0);
            return 0;

        case SYS_FORK:
            return fork(state);

        case SYS_EXEC:
        {
            if (!is_valid_user_mem(current_process->vmmc, (const void *)p0, p1, VMM_UR | VMM_PR))
            {
                current_process->tls->errno = EFAULT;
                return (uintptr_t)-EFAULT;
            }

            char *const *argv = (char *const *)p2;
            char *const *envp = (char *const *)p3;

            // TODO: Das ist schon ein bisschen aufwändig.
            char *const *ae[] = { argv, envp, NULL };
            for (int i = 0; ae[i] != NULL; i++)
            {
                for (int j = 0;; j++)
                {
                    if (!is_valid_user_mem(current_process->vmmc, &ae[i][j], sizeof(ae[i][j]), VMM_UR | VMM_PR))
                    {
                        current_process->tls->errno = EFAULT;
                        return (uintptr_t)-EFAULT;
                    }

                    if (ae[i][j] == NULL)
                        break;

                    if (!is_valid_user_string(ae[i][j]))
                    {
                        current_process->tls->errno = EFAULT;
                        return (uintptr_t)-EFAULT;
                    }
                }
            }

            return exec(state, (const void *)p0, p1, argv, envp);
        }

        case SYS_WAIT:
            if (!is_valid_user_mem(current_process->vmmc, (const void *)p1, sizeof(uintmax_t), VMM_UR | VMM_PW))
            {
                current_process->tls->errno = EFAULT;
                return -1;
            }

            return raw_waitpid(p0, (uintmax_t *)p1, p2, NULL, NULL);

        case SYS_HANDLE_IRQ:
            if (is_popup(current_process))
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

            register_user_isr(p0, current_process, (void (__attribute__((regparm(1))) *)(void *))p1, (void *)p2);
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

        case SYS_PHYS_ALLOC:
            return pmm_alloc_advanced(p0, p1, p2);

        case SYS_PHYS_FREE:
            for (uintptr_t phys = p0; phys < p0 + p1; phys += PAGE_SIZE)
                pmm_free(phys);
            return 0;

        case SYS_SLEEP:
            sleep(p0);
            return 0;

        case SYS_CREATE_THREAD:
        {
            if (IS_KERNEL(p0) || IS_KERNEL(p1))
            {
                current_process->tls->errno = EFAULT;
                return (uintptr_t)-EFAULT;
            }

            process_t *p = create_user_thread((void (*)(void *))p0, (void *)p1, (void *)p2);
            if (p != NULL)
                run_process(p);
            return p->pid;
        }

        case SYS_GETPGID:
        {
            process_t *p = find_process(p0);
            if (p == NULL)
            {
                current_process->tls->errno = ESRCH;
                return (uintptr_t)-1;
            }

            return p->pgid;
        }

        case SYS_ELAPSED_MS:
            return elapsed_ms;

        case SYS_VM86:
#ifdef X86
            if (!is_valid_user_mem(current_process->vmmc, (const void *)p1, sizeof(struct vm86_registers), VMM_UR | VMM_PR) ||
                (p3 && !is_valid_user_mem(current_process->vmmc, (const void *)p2, sizeof(struct vm86_memory_area) * p3, VMM_UR | VMM_PR)))
            {
                current_process->tls->errno = EFAULT;
                return (uintptr_t)-EFAULT;
            }

            vm86_interrupt(p0, (struct vm86_registers *)p1, (struct vm86_memory_area *)p2, p3);
            return 0;
#else
            current_process->tls->errno = ENOSYS;
            return (uintptr_t)-ENOSYS;
#endif

        /**
         * p0: Quellspeicheradresse
         * p1: Quellspeichergröße
         */
        case SYS_POPUP_SET_ANSWER:
        {
            if (!is_popup(current_process))
            {
                current_process->tls->errno = EPERM;
                return (uintptr_t)-EPERM;
            }

            if (!is_valid_user_mem(current_process->vmmc, (const void *)p0, p1, VMM_UR | VMM_PR))
            {
                current_process->tls->errno = EFAULT;
                return (uintptr_t)-EFAULT;
            }

            // TODO: Vielleicht nach process/management.c auslagern
            popup_return_memory_t *prm = kmalloc(sizeof(*prm));
            prm->parent = current_process->ppid;
            prm->size = p1;
            prm->mem = kmalloc(p1);
            prm->digest = calculate_checksum(*prm);

            memcpy(prm->mem, (const void *)p0, prm->size);

            if (current_process->popup_return != NULL)
            {
                kfree(current_process->popup_return->mem);
                kfree(current_process->popup_return);
            }

            current_process->popup_return = prm;

            return 0;
        }

        /**
         * p0: Antwort-ID (von SYS_IPC_POPUP zurückgegeben)
         * p1: Zielspeicheradresse
         * p2: Zielspeichergröße
         *
         * Rückgabewert: 0 in manchen Fehlerfällen oder Gesamtantwortlänge
         *
         * Hinweis: Löscht die Antwort im Kernelspace bei Erfolg und wenn
         *          p1 != NULL und p2 > 0
         */
        case SYS_POPUP_GET_ANSWER:
        {
            popup_return_memory_t *prm = (popup_return_memory_t *)p0;
            if (!is_valid_kernel_mem(prm, sizeof(*prm), VMM_PR | VMM_PW) || !check_integrity(*prm))
            {
                current_process->tls->errno = EFAULT;
                return 0;
            }

            if (prm->parent != current_process->pid)
            {
                current_process->tls->errno = EACCES;
                return 0;
            }

            size_t size = prm->size;

            if (p1 && p2 && !is_valid_user_mem(current_process->vmmc, (const void *)p1, p2, VMM_UR | VMM_PW))
                current_process->tls->errno = EFAULT;
            else if (p1 && p2)
            {
                memcpy((void *)p1, prm->mem, (p2 > prm->size) ? prm->size : p2);
                kfree(prm->mem);
                kfree(prm);
            }

            return size;
        }
    }

    current_process->tls->errno = ENOSYS;
    return 0;
}
