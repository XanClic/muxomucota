#include <compiler.h>
#include <ipc.h>
#include <shm.h>
#include <stdbool.h>
#include <stddef.h>
#include <vfs.h>


uintmax_t _vfs_is_symlink(uintptr_t shmid);

uintmax_t _vfs_is_symlink(uintptr_t shmid)
{
    size_t pathlen = popup_get_message(NULL, 0);

    char path[pathlen];
    popup_get_message(path, pathlen);


    void *dst = shm_open(shmid, VMM_UW);

    bool is_link = service_is_symlink(path, dst);

    shm_close(shmid, dst);


    return is_link;
}


bool service_is_symlink(const char *relpath, char *linkpath) cc_weak;

bool service_is_symlink(const char *relpath, char *linkpath)
{
    (void)relpath;
    (void)linkpath;

    return false;
}
