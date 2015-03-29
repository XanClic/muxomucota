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

#ifndef USB_HUBS_H
#define USB_HUBS_H

#include <stdint.h>

#include "usb.h"


struct usb_hub_descriptor {
    uint8_t b_length;
    uint8_t b_descriptor_type;
    uint8_t b_nbr_ports;
    uint16_t w_hub_characteristics;
    uint8_t b_pwr_on_2_pwr_good;
    uint8_t b_hub_contr_current;
    uint8_t device_removable[];
} __attribute__((packed));


enum {
    USB_DESC_HUB = 0x29,
};

// hub device features
enum {
    C_HUB_LOCAL_POWER = 0,
    C_HUB_OVER_CURRENT,
};

// hub port features
enum {
    PORT_CONNECTION = 0,
    PORT_ENABLE,
    PORT_SUSPEND,
    PORT_OVER_CURRENT,
    PORT_RESET,

    PORT_POWER = 8,
    PORT_LOW_SPEED,
    PORT_HIGH_SPEED,

    C_PORT_CONNECTION = 16,
    C_PORT_ENABLE,
    C_PORT_SUSPEND,
    C_PORT_OVER_CURRENT,
    C_PORT_RESET,
    PORT_TEST,
    PORT_INDICATOR,
};

typedef struct usb_hub_device {
    struct cdi_usb_hub cdi;
    usb_hub_t *hub;
} usb_hub_device_t;

#endif
