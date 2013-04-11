#ifndef _PRIME_PROCS_H
#define _PRIME_PROCS_H

#include <stddef.h>


int prime_process_count(void);
void fetch_prime_process(int index, void **start, size_t *length, char *name_array, size_t name_array_size);
void release_prime_process(int index, void *start, size_t length);

#endif
