#ifndef _IPC_H
#define _IPC_H

#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>
#include <sys/types.h>


#define MAX_POPUP_HANDLERS 32


struct ipc_syscall_params
{
    pid_t target_pid;
    int func_index;

    uintptr_t shmid;

    const void *short_message;
    size_t short_message_length;

    uintmax_t *synced_result;
};


void popup_entry(void (*entry)(int, uintptr_t));
noreturn void popup_exit(uintmax_t exit_info);

void popup_ping_handler(int index, uintmax_t (*handler)(void));
void popup_message_handler(int index, uintmax_t (*handler)(void));
void popup_shm_handler(int index, uintmax_t (*handler)(uintptr_t));

void register_irq_handler(int irq, void (*handler)(void *info), void *info);
noreturn void irq_handler_exit(void);


size_t popup_get_message(void *buffer, size_t buflen);

void ipc_ping(pid_t pid, int func_index);
uintmax_t ipc_ping_synced(pid_t pid, int func_index);

void ipc_message(pid_t pid, int func_index, const void *buffer, size_t length);
uintmax_t ipc_message_synced(pid_t pid, int func_index, const void *buffer, size_t length);

void ipc_shm(pid_t pid, int func_index, uintptr_t shmid);
uintmax_t ipc_shm_synced(pid_t pid, int func_index, uintptr_t shmid);

void ipc_shm_message(pid_t pid, int func_index, uintptr_t shmid, const void *buffer, size_t length);
uintmax_t ipc_shm_message_synced(pid_t pid, int func_index, uintptr_t shmid, const void *buffer, size_t length);


pid_t find_daemon_by_name(const char *name);


void yield_to(pid_t pid);
static inline void yield(void) { yield_to(-1); }

void msleep(int ms);


pid_t create_thread(void (*entry)(void *), void *stack_top, void *arg);

#endif
