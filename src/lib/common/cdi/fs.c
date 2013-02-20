#include <stddef.h>
#include <stdint.h>
#include <vfs.h>
#include <cdi.h>
#include <cdi/fs.h>


extern uintptr_t (*__cdi_create_pipe)(const char *relpath, int flags);
extern uintptr_t (*__cdi_duplicate_pipe)(uintptr_t id);

extern void (*__cdi_destroy_pipe)(uintptr_t id, int flags);


size_t cdi_fs_data_read(struct cdi_fs_filesystem *fs, uint64_t start, size_t size, void *buffer)
{
    pipe_set_flag(fs->osdep, F_POSITION, start);

    return stream_recv(fs->osdep, buffer, size, O_BLOCKING);
}


size_t cdi_fs_data_write(struct cdi_fs_filesystem *fs, uint64_t start, size_t size, const void *buffer)
{
    pipe_set_flag(fs->osdep, F_POSITION, start);

    return stream_send(fs->osdep, buffer, size, O_BLOCKING);
}


void cdi_fs_driver_init(struct cdi_fs_driver *driver)
{
    driver->drv.type = CDI_FILESYSTEM;
    cdi_driver_init((struct cdi_driver *)driver);
}


void cdi_fs_driver_destroy(struct cdi_fs_driver *driver)
{
    cdi_driver_destroy((struct cdi_driver *)driver);
}


static uintptr_t cdi_fs_create_pipe(const char *path, int flags)
{
    return 0;
}


static uintptr_t cdi_fs_duplicate_pipe(uintptr_t id)
{
    return 0;
}


static void cdi_fs_destroy_pipe(uintptr_t id, int flags)
{
}


void cdi_fs_driver_register(struct cdi_fs_driver *driver)
{
    __cdi_create_pipe    = cdi_fs_create_pipe;
    __cdi_duplicate_pipe = cdi_fs_duplicate_pipe;
    __cdi_destroy_pipe   = cdi_fs_destroy_pipe;

    driver->drv.init();
}
