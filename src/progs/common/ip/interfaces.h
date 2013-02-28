#ifndef INTERFACES_H
#define INTERFACES_H

#include <lock.h>
#include <stdint.h>


struct interface
{
    struct interface *next;

    lock_t lock;

    char *name;
    int fd;

    uint32_t ip;
};


struct interface *locate_interface(const char *name);
struct interface *find_interface(int fd);

#endif
