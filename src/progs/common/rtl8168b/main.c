/*
 * Copyright (c) 2007 The tyndur Project. All rights reserved.
 *
 * This code is derived from software contributed to the tyndur Project
 * by Patrick Pokatilo.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>

#include "cdi/net.h"
#include "cdi/pci.h"
#include "cdi/misc.h"

#include "rtl8168b.h"

#define DRIVER_NAME "rtl8168b"

static struct cdi_net_driver rtl8168b_driver;

static int rtl8168b_driver_init(void)
{
    cdi_net_driver_init(&rtl8168b_driver);

    return 0;
}

static int rtl8168b_driver_destroy(void)
{
    cdi_net_driver_destroy(&rtl8168b_driver);

    return 0;
}

static struct cdi_net_driver rtl8168b_driver = {
    .drv = {
        .name = DRIVER_NAME,
        .type = CDI_NETWORK,
        .bus = CDI_PCI,
        .init = rtl8168b_driver_init,
        .destroy = rtl8168b_driver_destroy,
        .init_device = rtl8168b_init_device,
        .remove_device = rtl8168b_remove_device,
    },

    .send_packet = rtl8168b_send_packet,
};

CDI_DRIVER(DRIVER_NAME, rtl8168b_driver)
