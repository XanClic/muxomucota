/*
 * Copyright (c) 2008 The tyndur Project. All rights reserved.
 *
 * This code is derived from software contributed to the tyndur Project
 * by Jörg Pfähler.
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
#include <stdlib.h>
#include <string.h>

#include "cdi/net.h"
#include "cdi/pci.h"
#include "cdi/misc.h"

#include "pcnet.h"

#define DRIVER_NAME "pcnet"

static struct cdi_net_driver pcnet_driver;

static int pcnet_driver_init(void)
{
    // TODO Auf pci-Service warten
    // TODO Auf tcpip-Service warten

    // Konstruktor der Vaterklasse
    cdi_net_driver_init(&pcnet_driver);

    return 0;
}

/**
 * Deinitialisiert die Datenstrukturen fuer den pcnet-Treiber
 */
static int pcnet_driver_destroy(void)
{
    cdi_net_driver_destroy(&pcnet_driver);

    // TODO Alle Karten deinitialisieren

    return 0;
}


static struct cdi_net_driver pcnet_driver = {
    .drv = {
        .name           = DRIVER_NAME,
        .type           = CDI_NETWORK,
        .bus            = CDI_PCI,
        .init           = pcnet_driver_init,
        .destroy        = pcnet_driver_destroy,
        .init_device    = pcnet_init_device,
        .remove_device  = pcnet_remove_device,
    },

    .send_packet        = pcnet_send_packet,
};

CDI_DRIVER(DRIVER_NAME, pcnet_driver)
