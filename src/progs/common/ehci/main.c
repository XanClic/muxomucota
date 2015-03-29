/******************************************************************************
 * Copyright (c) 2015 Max Reitz                                               *
 *                                                                            *
 * Permission is hereby granted,  free of charge,  to any person  obtaining a *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction,  including without limitation *
 * the rights to use, copy,  modify, merge, publish,  distribute, sublicense, *
 * and/or  sell copies of the  Software,  and to permit  persons to whom  the *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED,  INCLUDING BUT NOT LIMITED TO THE WARRANTIES  OF MERCHANTABILITY, *
 * FITNESS  FOR A PARTICULAR  PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL *
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER *
 * LIABILITY,  WHETHER IN AN ACTION OF CONTRACT,  TORT OR OTHERWISE,  ARISING *
 * FROM,  OUT OF  OR IN CONNECTION  WITH  THE SOFTWARE  OR THE  USE OR  OTHER *
 * DEALINGS IN THE SOFTWARE.                                                  *
 ******************************************************************************/

#include <cdi.h>
#include <cdi/usb_hcd.h>

#include "ehci.h"


#define DRIVER_NAME "ehci"


static struct cdi_usb_hcd ehcd;

static int ehcd_init(void)
{
    cdi_driver_init(&ehcd.drv);
    return 0;
}

static int ehcd_destroy(void)
{
    cdi_driver_destroy(&ehcd.drv);
    return 0;
}

static struct cdi_usb_hcd ehcd = {
    .drv = {
        .name           = DRIVER_NAME,
        .type           = CDI_USB_HCD,
        .bus            = CDI_PCI,
        .init           = ehcd_init,
        .destroy        = ehcd_destroy,
        .init_device    = ehci_init_device,
    },

    .rh_drv = {
        .port_disable       = ehci_rh_port_down,
        .port_reset_enable  = ehci_rh_port_up,
        .port_status        = ehci_rh_port_status,
    },

    .create_transaction     = ehci_create_transaction,
    .enqueue                = ehci_enqueue,
    .wait_transaction       = ehci_wait_transaction,
    .destroy_transaction    = ehci_destroy_transaction,
};

CDI_DRIVER(DRIVER_NAME, ehcd)
