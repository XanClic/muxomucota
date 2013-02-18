#ifndef _ARCH_LOCK_H
#define _ARCH_LOCK_H

#include <sys/types.h>


typedef struct
{
    int status;
    pid_t owner;
} lock_t;


#define LOCK_INITIALIZER { .status = 0 }

#define IS_UNLOCKED(l) ((l).status == 0)
#define IS_LOCKED(l)   ((l).status != 0)


static inline void lock_init(lock_t *lock) { lock->status = 0; }

#endif
