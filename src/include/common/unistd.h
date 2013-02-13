#ifndef _UNISTD_H
#define _UNISTD_H

#ifdef KERNEL
#include <cpu-state.h>
#endif
#include <sys/types.h>


#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2


#ifdef KERNEL
pid_t fork(struct cpu_state *current_state);
#else
pid_t fork(void);
#endif

int execvp(const char *file, char *const argv[]);

int chdir(const char *path);

pid_t getpid(void);

#endif
