#ifndef _DRIVERS_H
#define _DRIVERS_H

#include <stdbool.h>
#include <stdnoreturn.h>


noreturn void daemonize(const char *service, bool vfs);

#endif
