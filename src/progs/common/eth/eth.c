/************************************************************************
 * Copyright (C) 2013 by Hanna Reitz                                    *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

#include <compiler.h>
#include <drivers.h>
#include <ipc.h>
#include <lock.h>
#include <shm.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>
#include <arpa/inet.h>
#include <sys/stat.h>


struct network_packet
{
    struct network_packet *next;

    uint8_t src[6];
    int type;

    void *data;
    size_t length;
};


struct vfs_file
{
    enum
    {
        TYPE_REGISTER,
        TYPE_RECEIVE,
        TYPE_DIR,
        TYPE_CARD
    } type;

    union
    {
        struct
        {
            pid_t pid;
            struct network_card *card;
            uint8_t dest[6];
            int packet_type;

            struct vfs_file *next;

            struct network_packet *inqueue;
            rw_lock_t inqueue_lock;

            pid_t owner;
            bool signal;
        };

        struct
        {
            off_t pos;
            char *data;
            size_t sz;
        };
    };
};


struct network_card
{
    struct network_card *next;

    int nr;
    char *name;

    uint8_t mac[6];

    pid_t process;
    uintptr_t id;

    struct vfs_file *listeners;
    rw_lock_t listener_lock;
};

static struct network_card *cards = NULL;

static rw_lock_t card_lock = RW_LOCK_INITIALIZER;


struct ethernet_frame
{
    uint8_t dest[6], src[6];
    uint16_t type;
    char data[];
} cc_packed;


static bool eth_receive(struct network_card *card, const void *data, size_t length)
{
    if (length < sizeof(struct ethernet_frame))
    {
        fprintf(stderr, "(%s) short packet\n", card->name);
        return false;
    }


    const struct ethernet_frame *frame = data;

    if (memcmp(frame->dest, (uint8_t[6]){ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, sizeof(frame->dest)) &&
        memcmp(frame->dest, card->mac, sizeof(frame->dest)))
    {
        return false;
    }


    rwl_lock_r(&card->listener_lock);

    for (struct vfs_file *f = card->listeners; f != NULL; f = f->next)
    {
        struct network_packet *nnp = malloc(sizeof(*nnp));
        nnp->next = NULL;

        memcpy(nnp->src, frame->src, sizeof(nnp->src));

        nnp->length = length - sizeof(*frame);
        nnp->type   = ntohs(frame->type);

        nnp->data = malloc(nnp->length);
        memcpy(nnp->data, frame->data, nnp->length);


        rwl_lock_r(&f->inqueue_lock);

        struct network_packet **np;
        for (np = &f->inqueue; *np != NULL; np = &(*np)->next);

        rwl_require_w(&f->inqueue_lock);

        *np = nnp;

        rwl_unlock_w(&f->inqueue_lock);


        if (f->signal)
        {
            struct
            {
                pid_t pid;
                uintptr_t id;
            } msg = {
                .pid = getpgid(0),
                .id = (uintptr_t)f
            };

            ipc_message(f->owner, IPC_SIGNAL, &msg, sizeof(msg));
        }
    }

    rwl_unlock_r(&card->listener_lock);


    return true;
}


static int get_number(void)
{
    static int counter = 0;
    return __sync_fetch_and_add(&counter, 1);
}


uintptr_t service_create_pipe(const char *relpath, int flags)
{
    if (!*relpath)
    {
        if (flags & O_WRONLY)
        {
            errno = EACCES;
            return 0;
        }

        struct vfs_file *f = malloc(sizeof(*f));
        f->type = TYPE_DIR;
        f->pos  = 0;

        f->sz = 1;

        rwl_lock_r(&card_lock);

        for (struct network_card *c = cards; c != NULL; c = c->next)
            f->sz += strlen(c->name) + 1;

        char *d = f->data = malloc(f->sz);

        for (struct network_card *c = cards; c != NULL; c = c->next)
        {
            strcpy(d, c->name);
            d += strlen(c->name) + 1;
        }

        *d = 0;

        rwl_unlock_r(&card_lock);

        return (uintptr_t)f;
    }
    else if (!strcmp(relpath, "/register"))
    {
        if (flags & O_RDONLY)
        {
            errno = EACCES;
            return 0;
        }

        struct network_card *c = malloc(sizeof(*c));
        c->nr      = get_number();
        c->process = getpgid(getppid());
        c->id      = 0;

        c->listeners = NULL;

        rwl_lock_init(&c->listener_lock);

        asprintf(&c->name, "eth%i", c->nr);

        struct vfs_file *f = malloc(sizeof(*f));
        f->type = TYPE_REGISTER;
        f->card = c;

        return (uintptr_t)f;
    }
    else if (!strcmp(relpath, "/receive"))
    {
        if (flags & O_RDONLY)
        {
            errno = EACCES;
            return 0;
        }

        struct vfs_file *f = malloc(sizeof(*f));
        f->type = TYPE_RECEIVE;
        f->pid  = getpgid(getppid());
        f->card = NULL;

        return (uintptr_t)f;
    }
    else if (strchr(relpath + 1, '/') == NULL)
    {
        rwl_lock_r(&card_lock);

        struct network_card *c;
        for (c = cards; (c != NULL) && strcmp(c->name, relpath + 1); c = c->next);

        rwl_unlock_r(&card_lock);

        if (c == NULL)
        {
            errno = ENODEV;
            return 0;
        }

        struct vfs_file *f = malloc(sizeof(*f));
        f->type = TYPE_CARD;
        f->card = c;
        memset(f->dest, 0, sizeof(f->dest));

        f->packet_type = 0x0800;

        f->signal = false;

        f->inqueue = NULL;
        rwl_lock_init(&f->inqueue_lock);

        rwl_lock_w(&c->listener_lock);

        f->next = c->listeners;
        c->listeners = f;

        rwl_unlock_w(&c->listener_lock);

        return (uintptr_t)f;
    }
    else
    {
        char duped[strlen(relpath)];
        strcpy(duped, relpath + 1);

        char *card_name = strtok(duped, "/");

        rwl_lock_r(&card_lock);

        struct network_card *c;
        for (c = cards; (c != NULL) && strcmp(c->name, card_name); c = c->next);

        rwl_unlock_r(&card_lock);

        if (c == NULL)
        {
            errno = ENODEV;
            return 0;
        }

        char *cptr = strtok(NULL, "/");

        if (strtok(NULL, "/") != NULL)
        {
            errno = ENOENT;
            return 0;
        }

        uint8_t dest[6];

        for (int i = 0; i < 6; i++)
        {
            int val = strtol(cptr, &cptr, 16);
            if ((val < 0x00) || (val > 0xff) || ((*cptr != ':') && (i < 5)) || (*cptr && (i == 5)))
            {
                errno = ENOENT;
                return 0;
            }

            cptr++;

            dest[i] = val;
        }

        struct vfs_file *f = malloc(sizeof(*f));
        f->type = TYPE_CARD;
        f->card = c;
        memcpy(f->dest, dest, sizeof(f->dest));

        f->packet_type = 0x0800;

        f->signal = false;

        f->inqueue = NULL;
        rwl_lock_init(&f->inqueue_lock);

        rwl_lock_w(&c->listener_lock);

        f->next = c->listeners;
        c->listeners = f;

        rwl_unlock_w(&c->listener_lock);

        return (uintptr_t)f;
    }

    errno = ENODEV;
    return 0;
}


uintptr_t service_duplicate_pipe(uintptr_t id)
{
    struct vfs_file *f = (struct vfs_file *)id, *nf = malloc(sizeof(*f));

    memcpy(nf, f, sizeof(*f));

    if (f->type == TYPE_DIR)
    {
        nf->data = malloc(nf->sz);
        memcpy(nf->data, f->data, nf->sz);
    }
    else if (f->type == TYPE_CARD)
    {
        rwl_lock_w(&nf->card->listener_lock);

        nf->next = nf->card->listeners;
        nf->card->listeners = nf;

        rwl_unlock_w(&nf->card->listener_lock);

        rwl_lock_init(&nf->inqueue_lock);
        rwl_lock_r(&f->inqueue_lock);

        struct network_packet **endp = &nf->inqueue;

        for (struct network_packet *i = f->inqueue; i != NULL; i = i->next)
        {
            *endp = malloc(sizeof(*i));
            memcpy(*endp, i, sizeof(*i));

            (*endp)->data = malloc(i->length);
            memcpy((*endp)->data, i->data, i->length);

            endp = &(*endp)->next;
        }

        *endp = NULL;

        rwl_unlock_r(&f->inqueue_lock);
    }

    return (uintptr_t)nf;
}


void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    if (f->type == TYPE_DIR)
        free(f->data);
    else if (f->type == TYPE_CARD)
    {
        rwl_lock_w(&f->card->listener_lock);

        struct vfs_file **fp;
        for (fp = &f->card->listeners; (*fp != NULL) && (*fp != f); fp = &(*fp)->next);

        if (*fp != NULL)
            *fp = f->next;

        rwl_unlock_w(&f->card->listener_lock);


        struct network_packet *np = f->inqueue;

        while (np != NULL)
        {
            struct network_packet *nnp = np->next;

            free(np->data);
            free(np);

            np = nnp;
        }
    }

    free(f);
}


big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    switch (f->type)
    {
        case TYPE_REGISTER:
            if (size != sizeof(uintptr_t) + sizeof(uint8_t) * 6)
            {
                errno = EINVAL;
                return 0;
            }

            f->card->id = *(uintptr_t *)data;

            memcpy(f->card->mac, (uintptr_t *)data + 1, sizeof(f->card->mac));

            rwl_lock_w(&card_lock);

            f->card->next = cards;
            cards = f->card;

            rwl_unlock_w(&card_lock);

            return sizeof(uintptr_t);

        case TYPE_RECEIVE:
            if (f->card == NULL)
            {
                errno = ENXIO;
                return 0;
            }

            return eth_receive(f->card, data, size) ? size : 0;

        case TYPE_DIR:
            errno = EACCES;
            return 0;

        case TYPE_CARD:
        {
            if (size > 1500)
            {
                errno = EFBIG;
                return 0;
            }

            size_t frame_len = (size < 46) ? 46 : size;

            uintptr_t shmid = shm_create(sizeof(struct ethernet_frame) + frame_len);

            struct ethernet_frame *frame = shm_open(shmid, VMM_UW);

            memcpy(frame->dest, f->dest, sizeof(frame->dest));
            memcpy(frame->src, f->card->mac, sizeof(frame->src));

            frame->type = htons(f->packet_type);

            memcpy(frame->data, data, size);
            if (frame_len > size)
                memset(frame->data + size, 0, frame_len - size);

            big_size_t sent_size = sizeof(*frame) + frame_len;

            shm_close(shmid, frame, false);

            if (flags & O_NONBLOCK)
                ipc_shm_message(f->card->process, STREAM_SEND, shmid, &f->card->id, sizeof(f->card->id));
            else
                sent_size = ipc_shm_message_synced(f->card->process, STREAM_SEND, shmid, &f->card->id, sizeof(f->card->id));

            return sent_size;
        }
    }

    errno = EINVAL;
    return 0;
}


big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (f->type)
    {
        case TYPE_REGISTER:
        case TYPE_RECEIVE:
            errno = EACCES;
            return 0;

        case TYPE_DIR:
        {
            size_t copy_sz = (size > f->sz - (size_t)f->pos) ? (f->sz - (size_t)f->pos) : size;

            memcpy(data, f->data + f->pos, copy_sz);
            f->pos += copy_sz;

            return copy_sz;
        }

        case TYPE_CARD:
        {
            struct network_packet *np;

            do
            {
                rwl_lock_w(&f->inqueue_lock);

                np = f->inqueue;
                if (np != NULL)
                    f->inqueue = np->next;

                rwl_unlock_w(&f->inqueue_lock);

                if ((np == NULL) && !(flags & O_NONBLOCK))
                    yield();
            }
            while ((np == NULL) && !(flags & O_NONBLOCK));

            if (np == NULL)
                return 0;


            size_t copy_sz = (np->length < size) ? np->length : size;

            memcpy(data, np->data, copy_sz);

            free(np->data);
            free(np);


            return copy_sz;
        }
    }

    errno = EINVAL;
    return 0;
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (interface)
    {
        case I_FILE:
            return (f->type == TYPE_DIR);
        case I_STATABLE:
            return (f->type == TYPE_DIR) || (f->type == TYPE_CARD);
        case I_DEVICE_CONTACT:
            return (f->type == TYPE_RECEIVE);
        case I_ETHERNET:
        case I_SIGNAL:
            return (f->type == TYPE_CARD);
    }

    return false;
}


int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (flag)
    {
        case F_DEVICE_ID:
            if (f->type != TYPE_RECEIVE)
            {
                errno = EINVAL;
                return -EINVAL;
            }

            rwl_lock_r(&card_lock);

            struct network_card *c;
            for (c = cards; (c != NULL) && ((c->process != f->pid) || (c->id != value)); c = c->next);

            rwl_unlock_r(&card_lock);

            if (c == NULL)
            {
                errno = ENXIO;
                return -ENXIO;
            }

            f->card = c;

            return 0;

        case F_FLUSH:
            if ((f->type != TYPE_CARD) || (value != 1))
                return 0;

            rwl_lock_w(&f->inqueue_lock);

            struct network_packet *np = f->inqueue;
            if (np != NULL)
                f->inqueue = np->next;

            rwl_unlock_w(&f->inqueue_lock);

            free(np->data);
            free(np);

            return 0;

        case F_POSITION:
            if (f->type != TYPE_DIR)
            {
                errno = EINVAL;
                return -EINVAL;
            }

            f->pos = (value > f->sz) ? f->sz : value;

            return 0;

        case F_DEST_MAC:
            if (f->type != TYPE_CARD)
            {
                errno = EINVAL;
                return -EINVAL;
            }

            for (int i = 0; i < 6; i++, value >>= 8)
                f->dest[i] = value & 0xff;

            return 0;

        case F_ETH_PACKET_TYPE:
            if (f->type != TYPE_CARD)
            {
                errno = EINVAL;
                return -EINVAL;
            }

            f->packet_type = value;

            return 0;

        case F_IPC_SIGNAL:
            if (f->type != TYPE_CARD)
            {
                errno = EINVAL;
                return -EINVAL;
            }

            f->owner  = getpgid(getppid());
            f->signal = (bool)value;

            return 0;
    }

    errno = EINVAL;
    return -EINVAL;
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (flag)
    {
        case F_PRESSURE:
            if (f->type == TYPE_DIR)
                return f->sz - f->pos;
            else if (f->type == TYPE_CARD)
                return (f->inqueue != NULL) ? f->inqueue->length : 0;
            else
                return 0;

        case F_READABLE:
            if (f->type == TYPE_DIR)
                return (f->sz > f->pos);
            else if (f->type == TYPE_CARD)
                return (f->inqueue != NULL);
            else
                return false;

        case F_WRITABLE:
            return (f->type != TYPE_DIR);
        case F_POSITION:
            return (f->type == TYPE_DIR) ? f->pos : 0;
        case F_FILESIZE:
            return (f->type == TYPE_DIR) ? f->sz : 0;
        case F_INODE:
            return 0;
        case F_MODE:
            if (f->type == TYPE_DIR)
                return S_IFDIR | S_IFODEV | 0777;
            else if (f->type == TYPE_CARD)
                return S_IFODEV | 0666;
            else
            {
                errno = EINVAL;
                return 0;
            }

        case F_NLINK:
        case F_BLOCK_SIZE:
            return 1;

        case F_UID:
        case F_GID:
        case F_ATIME:
        case F_MTIME:
        case F_CTIME:
        case F_BLOCK_COUNT:
            return 0;

        case F_DEST_MAC:
        {
            if (f->type != TYPE_CARD)
            {
                errno = EINVAL;
                return 0;
            }

            uint64_t mac = 0;

            for (int i = 0; i < 6; i++)
                mac |= (uint64_t)f->dest[i] << (i * 8);

            return mac;
        }

        case F_SRC_MAC:
        {
            if (f->type != TYPE_CARD)
            {
                errno = EINVAL;
                return 0;
            }

            if (f->inqueue == NULL)
                return 0;

            uint64_t mac = 0;

            for (int i = 0; i < 6; i++)
                mac |= (uint64_t)f->inqueue->src[i] << (i * 8);

            return mac;
        }

        case F_MY_MAC:
        {
            if (f->type != TYPE_CARD)
            {
                errno = EINVAL;
                return 0;
            }

            uint64_t mac = 0;

            for (int i = 0; i < 6; i++)
                mac |= (uint64_t)f->card->mac[i] << (i * 8);

            return mac;
        }

        case F_ETH_PACKET_TYPE:
            return ((f->type == TYPE_CARD) && (f->inqueue != NULL)) ? f->inqueue->type : 0;

        case F_IPC_SIGNAL:
            return (f->type == TYPE_CARD) ? f->signal : false;
    }

    errno = EINVAL;
    return 0;
}


int main(void)
{
    daemonize("eth", true);
}
