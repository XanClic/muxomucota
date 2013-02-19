#ifndef _LOCK_H
#define _LOCK_H

#include <stdbool.h>
#include <sys/types.h>


#ifdef KERNEL

typedef pid_t lock_t;


#define LOCK_INITIALIZER 0
#define IS_UNLOCKED(l)   ((l) == 0)
#define IS_LOCKED(l)     ((l) != 0)


static inline void lock_init(lock_t *lock) { *lock = 0; }

#else

typedef pid_t lock_t;


#define LOCK_INITIALIZER 0
#define IS_UNLOCKED(l)   ((l) <= 0)
#define IS_LOCKED(l)     ((l) >  0)


static inline void lock_init(lock_t *lock) { *lock = 0; }

#endif


void lock(volatile lock_t *v);
void unlock(volatile lock_t *v);
bool trylock(volatile lock_t *v);

#endif
