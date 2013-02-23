/*
 * Copyright (c) 2007, 2008 The tyndur Project. All rights reserved.
 *
 * This code is derived from software contributed to the tyndur Project
 * by Kevin Wolf.
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cdi/net.h"
#include "cdi/pci.h"
#include "cdi/misc.h"

#include "device.h"

#define DRIVER_NAME "e1000"

static struct cdi_net_driver driver;

/**
 * Initialisiert die Datenstrukturen fuer den e1000-Treiber
 */
static int e1000_driver_init(void)
{
    // Konstruktor der Vaterklasse
    cdi_net_driver_init(&driver);

    return 0;
}

/**
 * Deinitialisiert die Datenstrukturen fuer den e1000-Treiber
 */
static int e1000_driver_destroy(void)
{
    cdi_net_driver_destroy(&driver);

    // TODO Alle Karten deinitialisieren

    return 0;
}


static struct cdi_net_driver driver = {
    .drv = {
        .name           = DRIVER_NAME,
        .type           = CDI_NETWORK,
        .bus            = CDI_PCI,
        .init           = e1000_driver_init,
        .destroy        = e1000_driver_destroy,
        .init_device    = e1000_init_device,
        .remove_device  = e1000_remove_device,
    },

    .send_packet        = e1000_send_packet,
};

CDI_DRIVER(DRIVER_NAME, driver)
