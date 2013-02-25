#include <stdint.h>
#include <vfs.h>
#include <sys/types.h>


int find_pipe(pid_t pid, uintptr_t id)
{
    for (int i = 0; i < __MFILE; i++)
        if ((_pipes[i].pid == pid) && (_pipes[i].id == id))
            return i;

    return -1;
}
