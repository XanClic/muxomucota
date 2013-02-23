/*
 * Copyright (c) 2007 The tyndur Project. All rights reserved.
 *
 * This code is derived from software contributed to the tyndur Project
 * by Matthew Iselin.
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
#include <string.h>

#include "cdi/net.h"
#include "cdi/pci.h"
#include "cdi/misc.h"

#include "ne2k.h"

#define DRIVER_NAME "ne2k"

static struct cdi_net_driver ne2k_driver;

static int ne2k_driver_init(void)
{
    // Konstruktor der Vaterklasse
    cdi_net_driver_init(&ne2k_driver);

    return 0;
}

/**
 * Deinitialisiert die Datenstrukturen fuer den ne2k-Treiber
 */
static int ne2k_driver_destroy(void)
{
    cdi_net_driver_destroy(&ne2k_driver);

    // TODO Alle Karten deinitialisieren

    return 0;
}


static struct cdi_net_driver ne2k_driver = {
    .drv = {
        .name           = DRIVER_NAME,
        .type           = CDI_NETWORK,
        .bus            = CDI_PCI,
        .init           = ne2k_driver_init,
        .destroy        = ne2k_driver_destroy,
        .init_device    = ne2k_init_device,
        .remove_device  = ne2k_remove_device,
    },

    .send_packet        = ne2k_send_packet,
};

CDI_DRIVER(DRIVER_NAME, ne2k_driver)
