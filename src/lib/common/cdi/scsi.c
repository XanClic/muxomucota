/************************************************************************
 * Copyright (C) 2013, 2015 by Max Reitz                                *
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

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cdi.h>
#include <cdi/misc.h>
#include <cdi/storage.h>
#include <cdi/scsi.h>


// #define DEBUG

#ifdef DEBUG
#define DPRINTF(...) printf(__VA_ARGS__)
#else
#define DPRINTF(...)
#endif


#define SCSI_REQUEST_SENSE 0x03
#define SCSI_INQUIRY       0x12
#define SCSI_READ_CAPACITY 0x25
#define SCSI_READ10        0x28
#define SCSI_WRITE10       0x2a
#define SCSI_READ12        0xa8

struct scsi_cmd {
    uint8_t operation;
    uint8_t lun;
} __attribute__((packed));

struct scsi_cmd_request_sense {
    struct scsi_cmd cmd;
    uint16_t rsvd2;
    uint8_t alloc_len;
    uint8_t control;
} __attribute__((packed));

struct scsi_cmd_inquiry {
    struct scsi_cmd cmd;
    uint8_t page;
    uint16_t alloc_len;
    uint8_t control;
} __attribute__((packed));

struct scsi_cmd_read_capacity {
    struct scsi_cmd cmd;
    uint32_t lba;
    uint32_t rsvd2;
} __attribute__((packed));

struct scsi_cmd_read10 {
    struct scsi_cmd cmd;
    uint32_t lba;
    uint8_t rsvd2;
    uint16_t length;
    uint8_t control;
} __attribute__((packed));

struct scsi_cmd_read12 {
    struct scsi_cmd cmd;
    uint32_t lba;
    uint32_t length;
    uint16_t rsvd2;
} __attribute__((packed));

struct scsi_cmd_write10 {
    struct scsi_cmd cmd;
    uint32_t lba;
    uint8_t rsvd2;
    uint16_t length;
    uint8_t control;
} __attribute__((packed));

struct scsi_sense {
    uint8_t resp_code;
    uint8_t segment;
    uint8_t flags;
    uint32_t information;
    uint8_t add_sense_length;
    uint32_t cmd_spec_info;
    uint8_t add_sense_code;
    uint8_t add_sense_code_qual;
    uint8_t field_replacable_unit_code;
    uint8_t sense_key_spec1;
    uint8_t sense_key_spec2;
    uint8_t sense_key_spec3;
} __attribute__((packed));

struct scsi_inq_answer {
    uint8_t dev_type;
    uint8_t type_mod;
    uint8_t version;
    uint8_t resp_data_format;
    uint8_t add_length;
    uint16_t rsvd2;
    uint8_t flags;
    char vendor[8];
    char product[16];
    uint32_t revision;
} __attribute__((packed));


extern void cdi_storage_driver_register(struct cdi_storage_driver *driver);

static struct cdi_storage_driver storage_drv;

typedef struct scsi_storage_device {
    struct cdi_storage_device dev;
    struct cdi_scsi_device *backend;
    int lun;
} scsi_storage_device_t;


void cdi_scsi_driver_init(struct cdi_scsi_driver *driver)
{
    cdi_driver_init((struct cdi_driver *)driver);
}


void cdi_scsi_driver_destroy(struct cdi_scsi_driver *driver)
{
    cdi_driver_destroy((struct cdi_driver *)driver);
}


void cdi_scsi_driver_register(struct cdi_scsi_driver *driver)
{
    (void)driver;
    cdi_storage_driver_register(&storage_drv);
}


static int scsi_get_capacity(scsi_storage_device_t *dev, uint64_t *block_count, size_t *block_size)
{
    struct cdi_scsi_driver *drv = CDI_UPCAST(dev->backend->dev.driver, struct cdi_scsi_driver, drv);
    uint32_t capacity[2];
    struct scsi_cmd_read_capacity read_cap = {
        .cmd = {
            .operation  = SCSI_READ_CAPACITY,
            .lun        = dev->lun
        }
    };
    struct cdi_scsi_packet pkt = {
        .buffer     = capacity,
        .bufsize    = sizeof(capacity),
        .direction  = CDI_SCSI_READ,
        .cmdsize    = sizeof(read_cap)
    };
    memcpy(pkt.command, &read_cap, sizeof(read_cap));

    if (drv->request(dev->backend, &pkt) < 0) {
        return -EIO;
    }

    *block_count = (uint64_t)cdi_be32_to_cpu(capacity[0]) + 1;
    *block_size  = cdi_be32_to_cpu(capacity[1]);

    return 0;
}


void cdi_scsi_device_init(struct cdi_scsi_device *device)
{
    static int dev_counter = 0;

    if (device->type != CDI_STORAGE) {
        return;
    }

    for (int lun = 0; lun < device->lun_count; lun++) {
        scsi_storage_device_t *dev = calloc(1, sizeof(*dev));
        dev->backend = device;
        dev->lun = lun;

        if (scsi_get_capacity(dev, &dev->dev.block_count, &dev->dev.block_size) < 0) {
            free(dev);
            continue;
        }

        DPRINTF("[scsi] %s.%i capacity: %llu MB\n", device->dev.name, lun,
                (dev->dev.block_count * dev->dev.block_size) / 1048576);

        dev->dev.dev.driver = &storage_drv.drv;
        dev->dev.dev.name = malloc(4);
        sprintf((char *)dev->dev.dev.name, "sd%c", 'a' + dev_counter++);

        cdi_storage_device_init(&dev->dev);
    }
}


static int scsi_read_blocks(struct cdi_storage_device *dev, uint64_t start, uint64_t count, void *buffer)
{
    scsi_storage_device_t *scsi_dev = CDI_UPCAST(dev, scsi_storage_device_t, dev);
    struct cdi_scsi_driver *drv = CDI_UPCAST(scsi_dev->backend->dev.driver, struct cdi_scsi_driver, drv);

    if ((start >> 32) || (count >> 16)) {
        return -EINVAL;
    }

    struct scsi_cmd_read10 r10 = {
        .cmd = {
            .operation  = SCSI_READ10,
            .lun        = scsi_dev->lun
        },
        .lba    = cdi_cpu_to_be32(start),
        .length = cdi_cpu_to_be16(count)
    };
    struct cdi_scsi_packet pkt = {
        .buffer     = buffer,
        .bufsize    = count * dev->block_size,
        .direction  = CDI_SCSI_READ,
        .cmdsize    = sizeof(r10)
    };
    memcpy(pkt.command, &r10, sizeof(r10));

    if (drv->request(scsi_dev->backend, &pkt) < 0) {
        return -EIO;
    }

    return 0;
}


static int scsi_write_blocks(struct cdi_storage_device *dev, uint64_t start, uint64_t count, void *buffer)
{
    scsi_storage_device_t *scsi_dev = CDI_UPCAST(dev, scsi_storage_device_t, dev);
    struct cdi_scsi_driver *drv = CDI_UPCAST(scsi_dev->backend->dev.driver, struct cdi_scsi_driver, drv);

    if ((start >> 32) || (count >> 16)) {
        return -EINVAL;
    }

    struct scsi_cmd_write10 w10 = {
        .cmd = {
            .operation  = SCSI_WRITE10,
            .lun        = scsi_dev->lun
        },
        .lba    = cdi_cpu_to_be32(start),
        .length = cdi_cpu_to_be16(count)
    };
    struct cdi_scsi_packet pkt = {
        .buffer     = buffer,
        .bufsize    = count * dev->block_size,
        .direction  = CDI_SCSI_WRITE,
        .cmdsize    = sizeof(w10)
    };
    memcpy(pkt.command, &w10, sizeof(w10));

    if (drv->request(scsi_dev->backend, &pkt) < 0) {
        return -EIO;
    }

    return 0;
}


static struct cdi_storage_driver storage_drv = {
    .drv = {
        .type   = CDI_STORAGE,
        .bus    = CDI_SCSI,
        .name   = "scsi-storage",
    },

    .read_blocks    = scsi_read_blocks,
    .write_blocks   = scsi_write_blocks,
};
