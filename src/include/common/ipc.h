#ifndef _IPC_H
#define _IPC_H

#include <stddef.h>
#include <stdnoreturn.h>
#include <sys/types.h>


#define MAX_POPUP_HANDLERS 32


void popup_entry(void (*entry)(void));
noreturn void popup_exit(void);

void popup_handler(int index, void (*handler)(void));

size_t popup_get_message(void *buffer, size_t buflen);

void ipc_message(pid_t pid, int func_index, const void *buffer, size_t length);


pid_t find_daemon_by_name(const char *name);

#endif
