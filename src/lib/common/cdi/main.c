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

#include <cdi.h>
#include <drivers.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <cdi.h>
#include <cdi/fs.h>
#include <cdi/lists.h>
#include <cdi/net.h>
#include <cdi/storage.h>
#include <drivers/ports.h>


extern struct cdi_driver *__start_cdi_drivers, *__stop_cdi_drivers;


extern void cdi_osdep_pci_collect(struct cdi_driver *drv);

extern void cdi_net_driver_register(struct cdi_net_driver *driver);
extern void cdi_storage_driver_register(struct cdi_storage_driver *driver);


void cdi_osdep_device_found(void);

static bool found_any = false;


int main(void)
{
    // Kann nicht so wirklich schaden. Wenn ein Treiber einen IRQ reserviert,
    // bevor die I/O-Ports reserviert werden, wird iopl() sonst erst aufgerufen,
    // nachdem der IRQ-Thread schon erstellt wurde, der dann noch IOPL=0 hat.
    iopl(3);


    if (&__start_cdi_drivers + 1 < &__stop_cdi_drivers)
    {
        fprintf(stderr, "Warning: Only one driver supported per binary. Ignoring the following:\n");

        for (struct cdi_driver **drvp = &__start_cdi_drivers + 1; drvp < &__stop_cdi_drivers; drvp++)
            fprintf(stderr, " - %s\n", (*drvp)->name);

        fprintf(stderr, "Running %s only.\n", __start_cdi_drivers->name);
    }

    bool vfs = false;

    switch ((int)__start_cdi_drivers->type)
    {
        case CDI_NETWORK:
            cdi_net_driver_register((struct cdi_net_driver *)__start_cdi_drivers);
            if (__start_cdi_drivers->init_device != NULL)
                cdi_osdep_pci_collect(__start_cdi_drivers);
            break;

        case CDI_FILESYSTEM:
            cdi_fs_driver_register((struct cdi_fs_driver *)__start_cdi_drivers);
            vfs = true;
            break;

        case CDI_STORAGE:
            cdi_storage_driver_register((struct cdi_storage_driver *)__start_cdi_drivers);
            if (__start_cdi_drivers->init_device != NULL)
                cdi_osdep_pci_collect(__start_cdi_drivers);
            vfs = true;
            break;

        default:
            fprintf(stderr, "Unknown driver type %i for %s.\n", __start_cdi_drivers->type, __start_cdi_drivers->name);
            return 1;
    }


    if ((__start_cdi_drivers->type == CDI_FILESYSTEM) || found_any)
        daemonize(__start_cdi_drivers->name, vfs);

    return 1;
}


void cdi_driver_init(struct cdi_driver *driver)
{
    driver->devices = cdi_list_create();
}

void cdi_driver_destroy(struct cdi_driver *driver)
{
    struct cdi_device *dev;

    while ((dev = cdi_list_pop(driver->devices)) != NULL)
    {
        if (driver->remove_device != NULL)
            driver->remove_device(dev);
        free(dev);
    }

    cdi_list_destroy(driver->devices);
}


void cdi_init(void)
{
}


void cdi_osdep_device_found(void)
{
    found_any = true;
}
