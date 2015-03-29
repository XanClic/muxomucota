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

#ifndef USB_H
#define USB_H

#include <stdbool.h>
#include <stdint.h>

#include <cdi.h>
#include <cdi/usb.h>
#include <cdi/usb_hcd.h>


typedef struct usb_logical_device usb_logical_device_t;

typedef struct usb_hub {
    struct cdi_usb_hub *cdi;

    const struct cdi_usb_hub_driver *drv;
    struct cdi_usb_hc *hc;
    usb_logical_device_t *ldev;
} usb_hub_t;

typedef struct usb_endpoint {
    int ep;
    cdi_usb_endpoint_type_t type;
    int mps;
    int toggle;
    struct cdi_usb_endpoint_descriptor *desc;
} usb_endpoint_t;

typedef struct usb_device {
    struct cdi_usb_hc *hc;
    int id;
    cdi_usb_speed_t speed;

    int tt_addr, tt_port;

    // First index: OUT (0) / IN (1)
    usb_endpoint_t eps[2][16];

    struct cdi_usb_device_descriptor dev_desc;
    struct cdi_usb_configuration_descriptor **configs, *config;
} usb_device_t;

struct usb_logical_device {
    struct cdi_usb_device cdi;
    usb_device_t *dev;
    int *endpoints;
};


struct cdi_device *usb_init_hc(struct cdi_bus_data *bus_data);

void usb_get_endpoint_descriptor(struct cdi_usb_device *dev, int ep,
                                 struct cdi_usb_endpoint_descriptor *desc);

cdi_usb_transmission_result_t
    usb_control_transfer(struct cdi_usb_device *dev, int ep,
                         const struct cdi_usb_setup_packet *setup, void *data);

cdi_usb_transmission_result_t usb_bulk_transfer(struct cdi_usb_device *dev,
                                                int ep, void *data,
                                                size_t size);

#endif
