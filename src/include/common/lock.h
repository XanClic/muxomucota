#ifndef _LOCK_H
#define _LOCK_H

#include <stdbool.h>

#include <arch-lock.h>


bool lock(volatile lock_t *v);
void unlock(volatile lock_t *v);

#endif
