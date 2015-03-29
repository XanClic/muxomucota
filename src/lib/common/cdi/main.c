/************************************************************************
 * Copyright (C) 2013 by Max Reitz                                      *
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

#include <assert.h>
#include <cdi.h>
#include <drivers.h>
#include <ipc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <usb-ipc.h>
#include <cdi.h>
#include <cdi/fs.h>
#include <cdi/lists.h>
#include <cdi/misc.h>
#include <cdi/net.h>
#include <cdi/scsi.h>
#include <cdi/storage.h>
#include <cdi/usb.h>
#include <drivers/ports.h>


extern struct cdi_driver *__start_cdi_drivers, *__stop_cdi_drivers;


extern void cdi_osdep_pci_collect(struct cdi_driver *drv);
extern void cdi_osdep_set_up_usb_dd_ipc(struct cdi_driver *drv);
extern void cdi_osdep_handle_usb_device(struct cdi_usb_bus_device_pattern *p);

extern void cdi_net_driver_register(struct cdi_net_driver *driver);
extern void cdi_storage_driver_register(struct cdi_storage_driver *driver);
extern void cdi_usb_driver_register(struct cdi_usb_driver *driver);

extern void cdi_osdep_provide_hc(struct cdi_usb_hc *hc);


void cdi_osdep_device_found(void);

static bool found_any = false;


static void provide_devices(struct cdi_driver *drv)
{
    switch ((int)drv->bus) {
        case CDI_PCI:
            cdi_osdep_pci_collect(drv);
            break;

        case CDI_USB:
            cdi_osdep_set_up_usb_dd_ipc(drv);
            break;
    }
}


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
    struct cdi_driver *drv = __start_cdi_drivers;

    drv->init();

    switch ((int)drv->type)
    {
        case CDI_NETWORK:
            cdi_net_driver_register(CDI_UPCAST(drv, struct cdi_net_driver, drv));
            break;

        case CDI_FILESYSTEM:
            cdi_fs_driver_register(CDI_UPCAST(drv, struct cdi_fs_driver, drv));
            vfs = true;
            break;

        case CDI_STORAGE:
            cdi_storage_driver_register(CDI_UPCAST(drv, struct cdi_storage_driver, drv));
            vfs = true;
            break;

        case CDI_SCSI:
            cdi_scsi_driver_register(CDI_UPCAST(drv, struct cdi_scsi_driver, drv));
            vfs = true;
            break;

        case CDI_USB:
            cdi_usb_driver_register(CDI_UPCAST(drv, struct cdi_usb_driver, drv));
            break;
    }

    if (drv->init_device != NULL) {
        provide_devices(drv);
    }

    if ((drv->type == CDI_FILESYSTEM) || (drv->type == CDI_USB) || (drv->bus == CDI_USB) || found_any) {
        daemonize(drv->name, vfs);
    }

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


void cdi_handle_bus_device(struct cdi_driver *drv, struct cdi_bus_device_pattern *pattern)
{
    (void)drv;

    switch ((int)pattern->bus_type) {
        case CDI_USB:
            cdi_osdep_handle_usb_device(CDI_UPCAST(pattern, struct cdi_usb_bus_device_pattern, pattern));
            break;
    }
}


void cdi_osdep_device_found(void)
{
    found_any = true;
}


extern void cdi_osdep_provide_usb_device_to(struct cdi_usb_device *dev, pid_t pid);

int cdi_provide_device(struct cdi_bus_data *device)
{
    if (device->bus_type != CDI_USB) {
        return -1;
    }

    struct cdi_usb_device *usb_dev = CDI_UPCAST(device, struct cdi_usb_device, bus_data);

    pid_t pid;
    char name[32];

    sprintf(name, "usb-%04x-%04x", usb_dev->vendor_id, usb_dev->product_id);
    pid = find_daemon_by_name(name);
    if (pid >= 0) {
        cdi_osdep_provide_usb_device_to(usb_dev, pid);
        return 0;
    }

    sprintf(name, "usb-%02x-%02x-%02x", usb_dev->class_id,
            usb_dev->subclass_id, usb_dev->protocol_id);
    pid = find_daemon_by_name(name);
    if (pid >= 0) {
        cdi_osdep_provide_usb_device_to(usb_dev, pid);
        return 0;
    }

    sprintf(name, "usb-%02x-%02x-?", usb_dev->class_id,
            usb_dev->subclass_id);
    pid = find_daemon_by_name(name);
    if (pid >= 0) {
        cdi_osdep_provide_usb_device_to(usb_dev, pid);
        return 0;
    }

    sprintf(name, "usb-%02x-?-?", usb_dev->class_id);
    pid = find_daemon_by_name(name);
    if (pid >= 0) {
        cdi_osdep_provide_usb_device_to(usb_dev, pid);
        return 0;
    }

    return -1;
}


void cdi_osdep_new_device(struct cdi_driver *drv, struct cdi_device *dev);

void cdi_osdep_new_device(struct cdi_driver *drv, struct cdi_device *dev)
{
    if (!dev) {
        return;
    }

    dev->driver = drv;
    cdi_list_push(drv->devices, dev);

    switch ((int)drv->type) {
        case CDI_USB_HCD:
            cdi_osdep_provide_hc(CDI_UPCAST(dev, struct cdi_usb_hc, dev));
            break;

        case CDI_SCSI:
            cdi_scsi_device_init(CDI_UPCAST(dev, struct cdi_scsi_device, dev));
            break;
    }
}
