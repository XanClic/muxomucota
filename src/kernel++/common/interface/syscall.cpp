#include <arch-constants.hpp>
#include <cerrno>
#include <cpu-state.hpp>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <ipc.hpp>
#include <isr.hpp>
#include <kassert.hpp>
#include <kmalloc.hpp>
#include <pmm.hpp>
#include <process.hpp>
#include <shm.hpp>
#include <syscall.hpp>
#include <system-timer.hpp>
#include <unistd.h>
#include <vmem.hpp>
#include <vm86.hpp>


using namespace mu;


extern volatile int elapsed_ms;


static bool is_valid_user_string(const char *s)
{
    uintptr_t addr = reinterpret_cast<uintptr_t>(s);

    for (;;) {
        ptrdiff_t page_data_size = PAGE_SIZE - (addr & PAGE_MASK);

        if (!current_process->vmm->is_valid_user_mem(reinterpret_cast<const void *>(addr), page_data_size, VMM_UR | VMM_PR)) {
            return false;
        }

        for (int i = 0; i < page_data_size; i++) {
            if (!reinterpret_cast<const char *>(addr)[i]) {
                return true;
            }
        }

        addr += page_data_size;
    }
}


uintptr_t syscall_krn(int syscall_nr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4, cpu_state *state)
{
    (void)p4;


    switch (syscall_nr) {
        case SYS_EXIT:
            current_process->destroy(p0);
            return 0;

        case SYS_MAP_MEMORY:
            // TODO: Checks, of course
            return reinterpret_cast<uintptr_t>(current_process->vmm->map(p0, p1, p2));

        case SYS_UNMAP_MEMORY:
            if (!current_process->vmm->is_valid_user_mem(reinterpret_cast<const void *>(p0), p1, VMM_UR | VMM_UW)) {
                current_process->tls->errno = EFAULT;
                return 0;
            }

            current_process->vmm->unmap(reinterpret_cast<void *>(p0), p1);
            return 0;

        case SYS_DAEMONIZE:
            kassert(!current_process->is_popup());

            if (!is_valid_user_string(reinterpret_cast<const char *>(p0))) {
                p0 = 0;
            }
            current_process->daemonize(reinterpret_cast<const char *>(p0));
            return 0;

        case SYS_POPUP_ENTRY:
            if (IS_KERNEL(p0)) {
                current_process->tls->errno = EFAULT;
                return 0;
            }

            current_process->popup_entry = reinterpret_cast<void (*)(void)>(p0);
            break;

        case SYS_POPUP_EXIT:
            if (!current_process->is_popup()) {
                current_process->tls->errno = EPERM;
                return 0;
            }

            if (!current_process->vmm->is_valid_user_mem(reinterpret_cast<const void *>(p0), sizeof(uintmax_t), VMM_UR | VMM_UW | VMM_PW)) {
                current_process->tls->errno = EFAULT;
                return 0;
            }

            current_process->destroy(*reinterpret_cast<uintmax_t *>(p0));
            current_process->tls->errno = EINVAL;
            return 0;

        case SYS_IPC_POPUP: {
            if (!current_process->vmm->is_valid_user_mem(reinterpret_cast<const void *>(p0), sizeof(ipc_syscall_params), VMM_UR | VMM_PR)) {
                current_process->tls->errno = EFAULT;
                return 0;
            }

            ipc_syscall_params *ipc_sp = reinterpret_cast<ipc_syscall_params *>(p0);

            shm_reference *ref = ipc_sp->shmid ? kernel_object::fetch<shm_reference>(ipc_sp->shmid) : nullptr;

            if (ipc_sp->shmid && !ref) {
                current_process->tls->errno = EINVAL;
                return 0;
            }
            if (ipc_sp->short_message && !current_process->vmm->is_valid_user_mem(ipc_sp->short_message, ipc_sp->short_message_length, VMM_UR | VMM_PR)) {
                current_process->tls->errno = EFAULT;
                return 0;
            }
            if (ipc_sp->synced_result && !current_process->vmm->is_valid_user_mem(ipc_sp->synced_result, sizeof(*ipc_sp->synced_result), VMM_UR | VMM_PW)) {
                current_process->tls->errno = EFAULT;
                return 0;
            }

            process *proc = process::find(ipc_sp->target_pid);
            if (!proc) {
                current_process->tls->errno = ESRCH;
                return 0;
            }

            pid_t ret = proc->popup(ipc_sp->func_index, *ref, ipc_sp->short_message, ipc_sp->short_message_length, ipc_sp->synced_result);
            if (ret < 0) {
                current_process->tls->errno = -ret;
                return 0;
            } else if (ipc_sp->synced_result) {
                int errno = 0;
                popup_return_memory *prm = nullptr;
                process *popup = process::find(ret);
                kassert_print(popup, "Could not find popup thread");

                popup->wait(ipc_sp->synced_result, 0, &errno, &prm);

                if (errno) {
                    current_process->tls->errno = errno;
                }

                // FIXME: This is a memleak, if SYS_POPUP_GET_ANSWER does not get called
                return reinterpret_cast<uintptr_t>(prm);
            }

            return 0;
        }

        case SYS_POPUP_GET_MESSAGE:
            if (!current_process->is_popup()) {
                current_process->tls->errno = EPERM;
                return 0;
            }

            if (p0 && p1 && !current_process->vmm->is_valid_user_mem(reinterpret_cast<const void *>(p0), p1, VMM_UR | VMM_PW)) {
                current_process->tls->errno = EFAULT;
            } else if (p0 && p1) {
                memcpy(reinterpret_cast<void *>(p0), current_process->ptmp, (current_process->ptmpsz > p1) ? p1 : current_process->ptmpsz);
            }

            return current_process->ptmpsz;

        case SYS_FIND_DAEMON_BY_NAME:
            if (!is_valid_user_string(reinterpret_cast<const char *>(p0))) {
                current_process->tls->errno = EFAULT;
                return -1;
            }

            return find_daemon_by_name(reinterpret_cast<const char *>(p0));

        case SYS_SHM_CREATE:
            return *new shm_reference(new shm_area(p0));

        case SYS_SHM_MAKE: {
            void **vaddr_list = reinterpret_cast<void **>(p1);
            int *page_count_list = reinterpret_cast<int *>(p2);

            if (!current_process->vmm->is_valid_user_mem(vaddr_list, sizeof(vaddr_list[0]) * p0, VMM_UR | VMM_PR) ||
                !current_process->vmm->is_valid_user_mem(page_count_list, sizeof(page_count_list[0]) * p0, VMM_UR | VMM_PR))
            {
                current_process->tls->errno = EFAULT;
                return 0;
            }

            int count = static_cast<int>(p0);
            ptrdiff_t offset = static_cast<ptrdiff_t>(p3);

            if ((count <= 0) || (offset < 0) || (offset >= PAGE_SIZE)) {
                current_process->tls->errno = EINVAL;
                return 0;
            }

            /*
            // FIXME: Something like this would be nice, but it has to work
            for (int i = 0; i < count; i++) {
                // FIXME: The user doesn't really need VMM_UR access, but just
                // any access
                if (!current_process->vmm->is_valid_user_mem(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(vaddr_list[i]) & ~PAGE_MASK), PAGE_SIZE, VMM_UR))
                {
                    current_process->tls->errno = EFAULT;
                    return 0;
                }
            }
            */

            return *new shm_reference(new shm_area(count, vaddr_list, page_count_list, offset));
        }

        case SYS_SHM_OPEN: {
            shm_reference *shm = kernel_object::fetch<shm_reference>(p0);
            if (!shm) {
                current_process->tls->errno = EINVAL;
                return 0;
            }

            return reinterpret_cast<uintptr_t>(shm->map(current_process->vmm, p1));
        }

        case SYS_SHM_CLOSE: {
            shm_reference *shm = kernel_object::fetch<shm_reference>(p0);
            if (!shm) {
                current_process->tls->errno = EINVAL;
                return 0;
            }

            shm->unmap();
            return 0;
        }

        case SYS_SHM_UNMAKE: {
            shm_reference *shm = kernel_object::fetch<shm_reference>(p0);
            if (!shm) {
                current_process->tls->errno = EINVAL;
                return 0;
            }

            delete shm;
            return 0;
        }

        case SYS_SHM_SIZE: {
            shm_reference *shm = kernel_object::fetch<shm_reference>(p0);
            if (!shm) {
                current_process->tls->errno = EINVAL;
                return 0;
            }

            return shm->shm->size;
        }

        case SYS_SBRK:
            return reinterpret_cast<uintptr_t>(current_process->vmm->sbrk(static_cast<ptrdiff_t>(p0)));

        case SYS_YIELD:
            yield_to(p0);
            return 0;

        case SYS_FORK:
#ifndef KERNEL
            return fork(state);
#endif

        case SYS_EXEC: {
            if (!current_process->vmm->is_valid_user_mem(reinterpret_cast<const void *>(p0), p1, VMM_UR | VMM_PR)) {
                current_process->tls->errno = EFAULT;
                return static_cast<uintptr_t>(-EFAULT);
            }

            char *const *argv = reinterpret_cast<char *const *>(p2);
            char *const *envp = reinterpret_cast<char *const *>(p3);

            // TODO: This is a bit complicated
            for (char *const *ae: {argv, envp}) {
                for (int i = 0;; i++) {
                    if (!current_process->vmm->is_valid_user_mem(&ae[i], sizeof(ae[i]), VMM_UR | VMM_PR)) {
                        current_process->tls->errno = EFAULT;
                        return static_cast<uintptr_t>(-EFAULT);
                    }

                    if (!ae[i]) {
                        break;
                    }

                    if (!is_valid_user_string(ae[i])) {
                        current_process->tls->errno = EFAULT;
                        return static_cast<uintptr_t>(-EFAULT);
                    }
                }
            }

            return exec(state, reinterpret_cast<const void *>(p0), p1, argv, envp);
        }

        case SYS_WAIT: {
            if (!current_process->vmm->is_valid_user_mem(reinterpret_cast<const void *>(p1), sizeof(uintmax_t), VMM_UR | VMM_PW)) {
                current_process->tls->errno = EFAULT;
                return static_cast<uintptr_t>(-1);
            }

            process *proc = process::find(p0);
            if (!proc) {
                current_process->tls->errno = ESRCH;
                return static_cast<uintptr_t>(-1);
            }

            return proc->wait(reinterpret_cast<uintmax_t *>(p1), p2, nullptr, nullptr);
        }

        case SYS_HANDLE_IRQ:
            if (current_process->is_popup()) {
                // popup threads may not register
                current_process->tls->errno = EPERM;
                return static_cast<uintptr_t>(-EPERM);
            }
            if (IS_KERNEL(p1)) {
                current_process->tls->errno = EFAULT;
                return static_cast<uintptr_t>(-EFAULT);
            }

            register_user_isr(p0, current_process, reinterpret_cast<void (__attribute__((regparm(1))) *)(void *)>(p1), reinterpret_cast<void *>(p2));
            return 0;

        case SYS_IRQ_HANDLER_EXIT:
            current_process->daemonize_from_irq_handler();
            return 0;

        case SYS_IOPL:
#ifdef X86
            if (p0 > 3) {
                current_process->tls->errno = EINVAL;
                return static_cast<uintptr_t>(-EINVAL);
            }

            state->kiopl(p0);
            return 0;
#else
            current_process->tls->errno = ENOSYS;
            return static_cast<uintptr_t>(-ENOSYS);
#endif

        case SYS_PHYS_ALLOC:
            return physptr::alloc(p0, p1, p2);

        case SYS_PHYS_FREE:
            // XXX: oh my gawd
            for (uintptr_t phys = p0; phys < p0 + p1; phys += PAGE_SIZE) {
                physptr(phys).free();
            }
            return 0;

        case SYS_SLEEP:
            sleep(p0);
            return 0;

        case SYS_CREATE_THREAD: {
            if (IS_KERNEL(p0) || IS_KERNEL(p1)) {
                current_process->tls->errno = EFAULT;
                return static_cast<uintptr_t>(-EFAULT);
            }

            process *p = process::user_thread(reinterpret_cast<void (*)(void *)>(p0), reinterpret_cast<void *>(p1), reinterpret_cast<void *>(p2));
            if (p) {
                p->run();
            }

            return p ? static_cast<uintptr_t>(p->pid) : 0;
        }

        case SYS_GETPGID: {
            process *p = process::find(p0);
            if (!p) {
                current_process->tls->errno = ESRCH;
                return static_cast<uintptr_t>(-1);
            }

            return p->pgid;
        }

        case SYS_ELAPSED_MS:
            return elapsed_ms;

        case SYS_VM86:
#ifdef X86
            if (!current_process->vmm->is_valid_user_mem(reinterpret_cast<const void *>(p1), sizeof(vm86_registers), VMM_UR | VMM_PR) ||
                (p3 && !current_process->vmm->is_valid_user_mem(reinterpret_cast<const void *>(p2), sizeof(vm86_memory_area) * p3, VMM_UR | VMM_PR)))
            {
                current_process->tls->errno = EFAULT;
                return static_cast<uintptr_t>(-EFAULT);
            }

            vm86_interrupt(p0, reinterpret_cast<vm86_registers *>(p1), reinterpret_cast<vm86_memory_area *>(p2), p3);
            return 0;
#else
            current_process->tls->errno = ENOSYS;
            return static_cast<uintptr_t>(-ENOSYS);
#endif

        /**
         * p0: Source buffer address
         * p1: Source buffer size
         */
        case SYS_POPUP_SET_ANSWER: {
            if (!current_process->is_popup()) {
                current_process->tls->errno = EPERM;
                return static_cast<uintptr_t>(-EPERM);
            }

            if (current_process->vmm->is_valid_user_mem(reinterpret_cast<const void *>(p0), p1, VMM_UR | VMM_PR)) {
                current_process->tls->errno = EFAULT;
                return static_cast<uintptr_t>(-EFAULT);
            }

            // TODO: Maybe put this into process/management.cpp
            popup_return_memory *prm = new popup_return_memory;
            prm->caller = current_process->ppid;
            prm->size = p1;
            prm->mem = kmalloc(p1);

            memcpy(prm->mem, reinterpret_cast<const void *>(p0), prm->size);

            if (current_process->prm) {
                kfree(current_process->prm->mem);
                delete current_process->prm;
            }

            current_process->prm = prm;

            return 0;
        }

        /**
         * p0: Answer ID (returned by SYS_IPC_POPUP)
         * p1: Destination buffer address
         * p2: Destination buffer size
         *
         * Return value: 0 in some error cases or total answer size
         *
         * Note: Deletes the answer in kernel space in case of success and if
         *       p1 != nullptr and p2 > 0
         */
        case SYS_POPUP_GET_ANSWER: {
            popup_return_memory *prm = kernel_object::fetch<popup_return_memory>(p0);
            if (!prm) {
                current_process->tls->errno = EFAULT;
                return 0;
            }

            if (prm->caller != current_process->pid) {
                current_process->tls->errno = EACCES;
                return 0;
            }

            size_t size = prm->size;

            if (p1 && p2 && !current_process->vmm->is_valid_user_mem(reinterpret_cast<const void *>(p1), p2, VMM_UR | VMM_PW)) {
                current_process->tls->errno = EFAULT;
            } else if (p1 && p2) {
                memcpy(reinterpret_cast<void *>(p1), prm->mem, (p2 > prm->size) ? prm->size : p2);
                kfree(prm->mem);
                delete prm;
            }

            return size;
        }
    }

    current_process->tls->errno = ENOSYS;
    return 0;
}
