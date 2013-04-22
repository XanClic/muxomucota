#include <ipc.h>
#include <stddef.h>
#include <stdint.h>
#include <syscall.h>
#include <sys/types.h>


void ipc_ping(pid_t pid, int func_index)
{
    ipc_shm_message(pid, func_index, 0, NULL, 0);
}


uintmax_t ipc_ping_synced(pid_t pid, int func_index)
{
    return ipc_shm_message_synced(pid, func_index, 0, NULL, 0);
}


uintmax_t ipc_ping_request(pid_t pid, int func_index, uintptr_t *answer_id)
{
    return ipc_shm_message_request(pid, func_index, 0, NULL, 0, answer_id);
}


void ipc_message(pid_t pid, int func_index, const void *buffer, size_t length)
{
    ipc_shm_message(pid, func_index, 0, buffer, length);
}


uintmax_t ipc_message_synced(pid_t pid, int func_index, const void *buffer, size_t length)
{
    return ipc_shm_message_synced(pid, func_index, 0, buffer, length);
}


uintmax_t ipc_message_request(pid_t pid, int func_index, const void *buffer, size_t length, uintptr_t *answer_id)
{
    return ipc_shm_message_request(pid, func_index, 0, buffer, length, answer_id);
}


void ipc_shm(pid_t pid, int func_index, uintptr_t shmid)
{
    ipc_shm_message(pid, func_index, shmid, NULL, 0);
}


uintmax_t ipc_shm_synced(pid_t pid, int func_index, uintptr_t shmid)
{
    return ipc_shm_message_synced(pid, func_index, shmid, NULL, 0);
}


uintmax_t ipc_shm_request(pid_t pid, int func_index, uintptr_t shmid, uintptr_t *answer_id)
{
    return ipc_shm_message_request(pid, func_index, shmid, NULL, 0, answer_id);
}


void ipc_shm_message(pid_t pid, int func_index, uintptr_t shmid, const void *buffer, size_t length)
{
    struct ipc_syscall_params ipc_sp = {
        .target_pid = pid,
        .func_index = func_index,

        .shmid = shmid,

        .short_message = buffer,
        .short_message_length = length,

        .synced_result = NULL
    };

    syscall1(SYS_IPC_POPUP, (uintptr_t)&ipc_sp);
}


uintmax_t ipc_shm_message_synced(pid_t pid, int func_index, uintptr_t shmid, const void *buffer, size_t length)
{
    return ipc_shm_message_request(pid, func_index, shmid, buffer, length, NULL);
}


uintmax_t ipc_shm_message_request(pid_t pid, int func_index, uintptr_t shmid, const void *buffer, size_t length, uintptr_t *answer_id)
{
    uintmax_t retval = 0;

    struct ipc_syscall_params ipc_sp = {
        .target_pid = pid,
        .func_index = func_index,

        .shmid = shmid,

        .short_message = buffer,
        .short_message_length = length,

        .synced_result = &retval
    };

    uintptr_t aid = syscall1(SYS_IPC_POPUP, (uintptr_t)&ipc_sp);

    if (answer_id != NULL)
        *answer_id = aid;
    else if (aid)
    {
        // Antwort verwerfen
        char t;
        popup_get_answer(aid, &t, sizeof(t));
    }

    return retval;
}
