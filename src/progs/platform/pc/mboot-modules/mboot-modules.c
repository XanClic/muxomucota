#include <assert.h>
#include <ctype.h>
#include <drivers.h>
#include <drivers/memory.h>
#include <errno.h>
#include <ipc.h>
#include <multiboot.h>
#include <shm.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>


struct module
{
    struct module *next;

    char *name;

    void *ptr;
    size_t size;

    off_t file_off;
};


static int module_count;
static struct module *first_module;


static uintmax_t vfs_open(void)
{
    size_t total_length = popup_get_message(NULL, 0);

    assert(total_length > sizeof(int));

    char in_data[total_length];

    popup_get_message(in_data, total_length);

    struct ipc_create_pipe *ipc_cp = (struct ipc_create_pipe *)in_data;


    struct module *mod;

    for (mod = first_module; (mod != NULL) && strcmp(mod->name, ipc_cp->relpath); mod = mod->next);


    return (uintptr_t)mod;
}


static uintmax_t vfs_close(void)
{
    return 0;
}


static uintmax_t vfs_dup(void)
{
    struct ipc_duplicate_pipe ipc_dp;

    int recv = popup_get_message(&ipc_dp, sizeof(ipc_dp));
    assert(recv == sizeof(ipc_dp));

    return ipc_dp.id;
}


static uintmax_t vfs_read(uintptr_t shmid)
{
    struct ipc_stream_recv ipc_sr;

    int recv = popup_get_message(&ipc_sr, sizeof(ipc_sr));
    assert(recv == sizeof(ipc_sr));

    void *dst = shm_open(shmid, VMM_UW);


    struct module *mod = (struct module *)ipc_sr.id;


    size_t copy_sz = ((int)ipc_sr.size <= mod->size - mod->file_off) ? ipc_sr.size : (size_t)(mod->size - mod->file_off);

    memcpy(dst, (uint8_t *)mod->ptr + (ptrdiff_t)mod->file_off, copy_sz);

    mod->file_off += copy_sz;


    shm_close(shmid, dst);


    return copy_sz;
}


static uintmax_t vfs_get_flag(void)
{
    struct ipc_pipe_get_flag ipc_pgf;

    int recv = popup_get_message(&ipc_pgf, sizeof(ipc_pgf));
    assert(recv == sizeof(ipc_pgf));


    struct module *mod = (struct module *)ipc_pgf.id;


    switch (ipc_pgf.flag)
    {
        case O_PRESSURE:
            return mod->size - mod->file_off;
        case O_POSITION:
            return mod->file_off;
        case O_FILESIZE:
            return mod->size;
        case O_READABLE:
            return (mod->size - mod->file_off) > 0;
        case O_WRITABLE:
            return false;
        case O_FLUSH:
            return 0;
    }


    return 0; // FIXME
}


static uintmax_t vfs_set_flag(void)
{
    struct ipc_pipe_set_flag ipc_psf;

    int recv = popup_get_message(&ipc_psf, sizeof(ipc_psf));
    assert(recv == sizeof(ipc_psf));


    struct module *mod = (struct module *)ipc_psf.id;


    switch (ipc_psf.flag)
    {
        case O_POSITION:
            mod->file_off = (ipc_psf.value > mod->size) ? mod->size : ipc_psf.value;
            return 0;
    }


    return (uintmax_t)-EINVAL;
}


int main(int argc, char *argv[])
{
    uintptr_t mbi_paddr = 0;

    for (int i = 0; i < argc; i++)
    {
        if (!strncmp(argv[i], "mbi=", 4))
        {
            for (int j = 0; j < 8; j++)
            {
                int hex_digit = argv[i][j + 4];

                hex_digit = isdigit(hex_digit) ? (hex_digit - '0') : (tolower(hex_digit) - 'a' + 10);

                mbi_paddr |= hex_digit << ((7 - j) * 4);
            }
        }
    }


    if (!mbi_paddr)
        return 1;


    struct multiboot_info *mbi = map_memory(mbi_paddr, sizeof(*mbi), VMM_UR);

    module_count = mbi->mods_count;
    struct multiboot_module *modules = map_memory(mbi->mods_addr, sizeof(modules[0]) * module_count, VMM_UR);

    unmap_memory(mbi, sizeof(*mbi));


    struct module **nxtptr = &first_module;

    for (int i = 0; i < module_count; i++)
    {
        *nxtptr = malloc(sizeof(**nxtptr));


        const char *src = map_memory(modules[i].string, 1024, VMM_UR); // 1k ought to be enough

        int j;
        for (j = 0; src[j] && (src[j] != ' ') && (j < 1024); j++);

        (*nxtptr)->name = malloc(j + 1);

        memcpy((*nxtptr)->name, src, j);
        (*nxtptr)->name[j] = 0;

        unmap_memory(src, 1024);


        (*nxtptr)->size = modules[i].mod_end - modules[i].mod_start;

        (*nxtptr)->ptr = malloc((*nxtptr)->size);

        const void *data = map_memory(modules[i].mod_start, (*nxtptr)->size, VMM_UR);
        memcpy((*nxtptr)->ptr, data, (*nxtptr)->size);
        unmap_memory(data, (*nxtptr)->size);


        (*nxtptr)->file_off = 0;


        nxtptr = &(*nxtptr)->next;
    }


    *nxtptr = NULL;


    popup_message_handler(CREATE_PIPE,    vfs_open);
    popup_message_handler(DESTROY_PIPE,   vfs_close);
    popup_message_handler(DUPLICATE_PIPE, vfs_dup);

    popup_shm_handler    (STREAM_RECV,    vfs_read);

    popup_message_handler(PIPE_GET_FLAG,  vfs_get_flag);
    popup_message_handler(PIPE_SET_FLAG,  vfs_set_flag);


    daemonize("mboot");
}
