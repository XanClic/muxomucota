#ifndef _IPC_H
#define _IPC_H

#include <stdnoreturn.h>
#include <sys/types.h>


void popup_entry(void (*entry)(void));
noreturn void popup_exit(void);

void ipc_message(pid_t pid);

#endif
