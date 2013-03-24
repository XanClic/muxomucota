#include <stddef.h>
#include <time.h>
#include <vfs.h>

time_t time(time_t *t)
{
    time_t var = 0;

    if (t != NULL)
        *t = var;

    return var;
}
