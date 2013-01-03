#ifndef _SYS__WAIT_H
#define _SYS__WAIT_H

#include <sys/types.h>


#define WIFEXITED(v)   1
#define WEXITSTATUS(v) v

#define WNOHANG (1 << 0)


pid_t wait(int *stat_loc);
pid_t waitpid(pid_t pid, int *stat_loc, int options);

#endif
