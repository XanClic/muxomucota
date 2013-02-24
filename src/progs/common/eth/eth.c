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

struct network_card
{
    struct network_card *next;

    int nr;
    char *name;

    uint8_t mac[6];

    pid_t process;
    uintptr_t id;

    struct network_packet *inqueue;
    lock_t inqueue_lock;
};

static struct network_card *cards = NULL;

static lock_t card_lock = LOCK_INITIALIZER;


struct ethernet_frame
{
    uint8_t dest[6], src[6];
    uint16_t type;
    char data[];
} cc_packed;


static bool eth_receive(struct network_card *card, const void *data, size_t length)
{
    if (length < 60)
    {
        fprintf(stderr, "(%s) short packet\n", card->name);
        return false;
    }


    const struct ethernet_frame *frame = data;

    if (memcmp(frame->dest, (uint8_t[6]){ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, sizeof(frame->dest)) ||
        memcmp(frame->dest, card->mac, sizeof(frame->dest)))
    {
        return false;
    }


    struct network_packet *nnp = malloc(sizeof(*nnp));
    nnp->next = NULL;

    memcpy(nnp->src, frame->src, sizeof(nnp->src));

    nnp->length = length - sizeof(*frame);
    nnp->type   = ntohs(frame->type);

    nnp->data = malloc(nnp->length);
    memcpy(nnp->data, frame->data, nnp->length);


    lock(&card->inqueue_lock);

    struct network_packet **np;
    for (np = &card->inqueue; *np != NULL; np = &(*np)->next);

    *np = nnp;

    unlock(&card->inqueue_lock);


    return true;
}


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
        };

        struct
        {
            off_t pos;
            char *data;
            size_t sz;
        };
    };
};


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
            return 0;

        struct vfs_file *f = malloc(sizeof(*f));
        f->type = TYPE_DIR;
        f->pos  = 0;

        f->sz = 1;

        lock(&card_lock);

        for (struct network_card *c = cards; c != NULL; c = c->next)
            f->sz += strlen(c->name) + 1;

        char *d = f->data = malloc(f->sz);

        for (struct network_card *c = cards; c != NULL; c = c->next)
        {
            strcpy(d, c->name);
            d += strlen(c->name) + 1;
        }

        *d = 0;

        unlock(&card_lock);

        return (uintptr_t)f;
    }
    else if (!strcmp(relpath, "/register"))
    {
        if (flags & O_RDONLY)
            return 0;

        struct network_card *c = malloc(sizeof(*c));
        c->nr      = get_number();
        c->process = getppid();
        c->id      = 0;

        c->inqueue = NULL;

        lock_init(&c->inqueue_lock);

        asprintf(&c->name, "eth%i", c->nr);

        struct vfs_file *f = malloc(sizeof(*f));
        f->type = TYPE_REGISTER;
        f->card = c;

        return (uintptr_t)f;
    }
    else if (!strcmp(relpath, "/receive"))
    {
        if (flags & O_RDONLY)
            return 0;

        struct vfs_file *f = malloc(sizeof(*f));
        f->type = TYPE_RECEIVE;
        f->pid  = getppid();
        f->card = NULL;

        return (uintptr_t)f;
    }
    else if (strchr(relpath + 1, '/') == NULL)
    {
        lock(&card_lock);

        struct network_card *c;
        for (c = cards; (c != NULL) && strcmp(c->name, relpath + 1); c = c->next);

        unlock(&card_lock);

        if (c == NULL)
            return 0;

        struct vfs_file *f = malloc(sizeof(*f));
        f->type = TYPE_CARD;
        f->card = c;
        memset(f->dest, 0, sizeof(f->dest));

        f->packet_type = 0x0800;

        return (uintptr_t)f;
    }
    else
    {
        char duped[strlen(relpath)];
        strcpy(duped, relpath + 1);

        char *card_name = strtok(duped, "/");

        lock(&card_lock);

        struct network_card *c;
        for (c = cards; (c != NULL) && strcmp(c->name, card_name); c = c->next);

        unlock(&card_lock);

        if (c == NULL)
            return 0;

        char *cptr = strtok(NULL, "/");

        if (strtok(NULL, "/") != NULL)
            return 0;

        uint8_t dest[6];

        for (int i = 0; i < 6; i++)
        {
            int val = strtol(cptr, &cptr, 16);
            if ((val < 0x00) || (val > 0xff) || ((*cptr != ':') && (i < 5)) || (*cptr && (i == 5)))
                return 0;

            cptr++;

            dest[i] = val;
        }

        struct vfs_file *f = malloc(sizeof(*f));
        f->type = TYPE_CARD;
        f->card = c;
        memcpy(f->dest, dest, sizeof(f->dest));

        f->packet_type = 0x0800;

        return (uintptr_t)f;
    }

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

    return (uintptr_t)nf;
}


void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    if (f->type == TYPE_DIR)
        free(f->data);

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
                return 0;

            f->card->id = *(uintptr_t *)data;

            memcpy(f->card->mac, (uintptr_t *)data + 1, sizeof(f->card->mac));

            lock(&card_lock);

            f->card->next = cards;
            cards = f->card;

            unlock(&card_lock);

            return sizeof(uintptr_t);

        case TYPE_RECEIVE:
            if (f->card == NULL)
                return 0;

            return eth_receive(f->card, data, size) ? size : 0;

        case TYPE_DIR:
            return 0;

        case TYPE_CARD:
        {
            if (size > 1500)
                return 0;

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

            if (flags & O_NONBLOCK)
                ipc_shm_message(f->card->process, STREAM_SEND, shmid, &f->card->id, sizeof(f->card->id));
            else
                sent_size = ipc_shm_message_synced(f->card->process, STREAM_SEND, shmid, &f->card->id, sizeof(f->card->id));

            shm_close(shmid, frame);

            return sent_size;
        }
    }

    return 0;
}


big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    switch (f->type)
    {
        case TYPE_REGISTER:
        case TYPE_RECEIVE:
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
            if (size < 6)
                return 0;

            lock(&f->card->inqueue_lock);

            struct network_packet *np = f->card->inqueue;
            if (np != NULL)
                f->card->inqueue = np->next;

            unlock(&f->card->inqueue_lock);

            memcpy(data, np->src, sizeof(np->src));

            size_t copy_sz = (np->length < size - 6) ? np->length : (size - 6);

            memcpy((char *)data + sizeof(np->src), np->data, copy_sz);

            free(np->data);
            free(np);

            return copy_sz;
        }
    }

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
                return -EINVAL;

            lock(&card_lock);

            struct network_card *c;
            for (c = cards; (c != NULL) && ((c->process != f->pid) || (c->id != value)); c = c->next);

            unlock(&card_lock);

            if (c == NULL)
                return -ENXIO;

            f->card = c;

            return 0;

        case F_FLUSH:
            return 0;

        case F_POSITION:
            if (f->type != TYPE_DIR)
                return -EINVAL;

            f->pos = (value > f->sz) ? f->sz : value;

            return 0;

        case F_DEST_MAC:
            if (f->type != TYPE_CARD)
                return -EINVAL;

            for (int i = 0; i < 6; i++)
                f->dest[i] = (value >> ((5 - i) * 8)) & 0xff; // Big Endian

            return 0;

        case F_ETH_PACKET_TYPE:
            if (f->type != TYPE_CARD)
                return -EINVAL;

            f->packet_type = value;

            return 0;
    }

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
                return (f->card->inqueue != NULL) ? (f->card->inqueue->length + 6) : 0;
            else
                return 0;

        case F_READABLE:
            if (f->type == TYPE_DIR)
                return (f->sz > f->pos);
            else if (f->type == TYPE_CARD)
                return (f->card->inqueue != NULL);
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
                return S_IFDIR | S_IFODEV | S_IRWXU | S_IRWXG | S_IRWXO;
            else if (f->type == TYPE_CARD)
                return S_IFODEV | S_IRWXU | S_IRWXG | S_IRWXO;
            else
                return 0;

        case F_NLINK:
            return 1;
        case F_UID:
        case F_GID:
        case F_ATIME:
        case F_MTIME:
        case F_CTIME:
            return 0;
        case F_BLOCK_SIZE:
            return 1;
        case F_BLOCK_COUNT:
            return (f->type == TYPE_DIR) ? f->sz : 0;

        case F_DEST_MAC:
        {
            if (f->type != TYPE_CARD)
                return 0;

            uint64_t mac = 0;

            for (int i = 0; i < 6; i++)
                mac |= (uint64_t)f->dest[i] << ((5 - i) * 8); // Big Endian

            return mac;
        }

        case F_MY_MAC:
        {
            if (f->type != TYPE_CARD)
                return 0;

            uint64_t mac = 0;

            for (int i = 0; i < 6; i++)
                mac |= (uint64_t)f->card->mac[i] << ((5 - i) * 8); // Big Endian

            return mac;
        }

        case F_ETH_PACKET_TYPE:
            return (f->type == TYPE_CARD) ? f->packet_type : 0;
    }

    return 0;
}


int main(void)
{
    daemonize("eth", true);
}
