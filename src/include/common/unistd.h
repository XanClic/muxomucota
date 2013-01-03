#ifndef _UNISTD_H
#define _UNISTD_H

#include <sys/types.h>


#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2


pid_t fork(void);

int execvp(const char *file, char *const argv[]);

#endif
