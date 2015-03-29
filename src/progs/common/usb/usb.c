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

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cdi.h>
#include <cdi/misc.h>
#include <cdi/usb.h>
#include <cdi/usb_hcd.h>

#include "usb.h"
#include "usb-hubs.h"


// #define DEBUG

#ifdef DEBUG
#define DPRINTF(...) printf(__VA_ARGS__)
#else
#define DPRINTF(...)
#endif


static int current_usb_addr = 1;


static cdi_usb_transmission_result_t
    do_control_transfer(struct cdi_usb_hc *hc, int dev, cdi_usb_speed_t speed,
                        int tt_addr, int tt_port, const usb_endpoint_t *ep,
                        const struct cdi_usb_setup_packet *setup, void *data)
{
    struct cdi_usb_hcd *hcd = CDI_UPCAST(hc->dev.driver, struct cdi_usb_hcd,
                                         drv);
    cdi_usb_hc_transaction_t ta;
    cdi_usb_transmission_result_t r1, r2, r3;

    ta = hcd->create_transaction(hc, &(struct cdi_usb_hc_ep_info){
            .dev        = dev,
            .ep         = ep->ep,
            .ep_type    = ep->type,
            .speed      = speed,
            .mps        = ep->mps,
            .tt_addr    = tt_addr,
            .tt_port    = tt_port
        });

    hcd->enqueue(hc, &(struct cdi_usb_hc_transmission){
            .ta     = ta,
            .token  = CDI_USB_SETUP,
            .buffer = (void *)setup,
            .size   = sizeof(*setup),
            .toggle = 0,
            .result = &r1
        });

    if (data) {
        hcd->enqueue(hc, &(struct cdi_usb_hc_transmission){
                .ta     = ta,
                .token  = setup->bm_request_type & CDI_USB_CREQ_IN
                        ? CDI_USB_IN : CDI_USB_OUT,
                .buffer = data,
                .size   = setup->w_length,
                .toggle = 1,
                .result = &r2
            });
    } else {
        r2 = CDI_USB_OK;
    }

    hcd->enqueue(hc, &(struct cdi_usb_hc_transmission){
            .ta     = ta,
            .token  = setup->bm_request_type & CDI_USB_CREQ_IN ? CDI_USB_OUT
                                                               : CDI_USB_IN,
            .toggle = 1,
            .result = &r3
        });

    hcd->wait_transaction(hc, ta);
    hcd->destroy_transaction(hc, ta);

    return r1 ? r1 : r2 ? r2 : r3;
}

static cdi_usb_transmission_result_t
    control_transfer(const usb_device_t *dev, const usb_endpoint_t *ep,
                     const struct cdi_usb_setup_packet *setup, void *data)
{
    return do_control_transfer(dev->hc, dev->id, dev->speed, dev->tt_addr,
                               dev->tt_port, ep, setup, data);
}


static int get_mps0(usb_hub_t *hub, int hub_port, cdi_usb_speed_t speed)
{
    cdi_usb_transmission_result_t res;
    struct cdi_usb_device_descriptor device_descriptor;
    int tt_addr = 0, tt_port = 0;

    if (speed < CDI_USB_HIGH_SPEED && hub->ldev) {
        if (hub->ldev->dev->speed < CDI_USB_HIGH_SPEED) {
            tt_addr = hub->ldev->dev->tt_addr;
            tt_port = hub->ldev->dev->tt_port;
        } else {
            tt_addr = hub->ldev->dev->id;
            tt_port = hub_port;
        }
    }

    res = do_control_transfer(hub->hc, 0, speed, tt_addr, tt_port,
        &(usb_endpoint_t){
            .type       = CDI_USB_CONTROL,
            .mps        = 8
        }, &(struct cdi_usb_setup_packet){
            .bm_request_type    = CDI_USB_CREQ_DEVICE | CDI_USB_CREQ_IN,
            .b_request          = CDI_USB_CREQ_GET_DESCRIPTOR,
            .w_value            = (CDI_USB_DESC_DEVICE << 8) | 0,
            .w_length           = 8
        }, &device_descriptor);

    if (res != CDI_USB_OK) {
        return -EIO;
    }

    return device_descriptor.b_max_packet_size_0;
}


static usb_device_t *enumerate(usb_hub_t *hub, int hub_port,
                               cdi_usb_speed_t speed, int mps0)
{
    cdi_usb_transmission_result_t res;
    usb_device_t *dev = calloc(1, sizeof(*dev));

    int new_id = current_usb_addr++;
    dev->hc = hub->hc;
    dev->speed = speed;

    if (speed < CDI_USB_HIGH_SPEED && hub->ldev) {
        if (hub->ldev->dev->speed < CDI_USB_HIGH_SPEED) {
            dev->tt_addr = hub->ldev->dev->tt_addr;
            dev->tt_port = hub->ldev->dev->tt_port;
        } else {
            dev->tt_addr = hub->ldev->dev->id;
            dev->tt_port = hub_port;
        }
    }

    dev->eps[0][0].type = CDI_USB_CONTROL;
    dev->eps[0][0].mps  = mps0;

    res = control_transfer(dev, &dev->eps[0][0], &(struct cdi_usb_setup_packet){
            .bm_request_type    = CDI_USB_CREQ_DEVICE | CDI_USB_CREQ_OUT,
            .b_request          = CDI_USB_CREQ_SET_ADDRESS,
            .w_value            = new_id
        }, NULL);
    if (res != CDI_USB_OK) {
        free(dev);
        return NULL;
    }

    dev->id = new_id;

    res = control_transfer(dev, &dev->eps[0][0], &(struct cdi_usb_setup_packet){
            .bm_request_type    = CDI_USB_CREQ_DEVICE | CDI_USB_CREQ_IN,
            .b_request          = CDI_USB_CREQ_GET_DESCRIPTOR,
            .w_value            = (CDI_USB_DESC_DEVICE << 8) | 0,
            .w_length           = sizeof(dev->dev_desc)
        }, &dev->dev_desc);
    if (res != CDI_USB_OK) {
        free(dev);
        return NULL;
    }

    return dev;
}


#ifdef DEBUG
static void get_string_descriptor(usb_device_t *dev, int index,
                                  struct cdi_usb_string_descriptor *dst)
{
    cdi_usb_transmission_result_t res;

    res = control_transfer(dev, &dev->eps[0][0], &(struct cdi_usb_setup_packet){
            .bm_request_type    = CDI_USB_CREQ_DEVICE | CDI_USB_CREQ_IN,
            .b_request          = CDI_USB_CREQ_GET_DESCRIPTOR,
            .w_value            = (CDI_USB_DESC_STRING << 8) | index,
            .w_length           = 1
        }, dst);
    if (res != CDI_USB_OK || !dst->b_length) {
        dst->b_length = 0;
        return;
    }

    res = control_transfer(dev, &dev->eps[0][0], &(struct cdi_usb_setup_packet){
            .bm_request_type    = CDI_USB_CREQ_DEVICE | CDI_USB_CREQ_IN,
            .b_request          = CDI_USB_CREQ_GET_DESCRIPTOR,
            .w_value            = (CDI_USB_DESC_STRING << 8) | index,
            .w_length           = dst->b_length
        }, dst);
    if (res != CDI_USB_OK) {
        dst->b_length = 0;
        return;
    }
}
#endif


static int get_configurations(usb_device_t *dev)
{
    cdi_usb_transmission_result_t res;
    struct cdi_usb_configuration_descriptor config;

    dev->configs = calloc(1, dev->dev_desc.b_num_configurations
                             * sizeof(*dev->configs));

    for (int i = 0; i < (int)dev->dev_desc.b_num_configurations; i++) {
        res = control_transfer(dev, &dev->eps[0][0],
            &(struct cdi_usb_setup_packet){
                .bm_request_type    = CDI_USB_CREQ_DEVICE | CDI_USB_CREQ_IN,
                .b_request          = CDI_USB_CREQ_GET_DESCRIPTOR,
                .w_value            = (CDI_USB_DESC_CONFIGURATION << 8) | i,
                .w_length           = sizeof(config)
            }, &config);
        if (res != CDI_USB_OK) {
            goto fail;
        }

        dev->configs[i] = malloc(config.w_total_length);
        res = control_transfer(dev, &dev->eps[0][0],
            &(struct cdi_usb_setup_packet){
                .bm_request_type    = CDI_USB_CREQ_DEVICE | CDI_USB_CREQ_IN,
                .b_request          = CDI_USB_CREQ_GET_DESCRIPTOR,
                .w_value            = (CDI_USB_DESC_CONFIGURATION << 8) | i,
                .w_length           = config.w_total_length
            }, dev->configs[i]);
        if (res != CDI_USB_OK) {
            goto fail;
        }
    }

    return 0;

fail:
    for (int i = 0; i < (int)dev->dev_desc.b_num_configurations; i++) {
        free(dev->configs[i]);
    }
    free(dev->configs);
    return -EIO;
}


static bool compare_configurations(usb_device_t *dev, int i, int j)
{
    // TODO: Prefer self-powered
    if (dev->configs[i]->b_max_power != dev->configs[j]->b_max_power) {
        return dev->configs[i]->b_max_power > dev->configs[j]->b_max_power;
    }

    if (dev->configs[i]->b_num_interfaces != dev->configs[j]->b_num_interfaces)
    {
        return dev->configs[i]->b_num_interfaces
               > dev->configs[j]->b_num_interfaces;
    }

    return false;
}


static int choose_configuration(usb_device_t *dev)
{
    cdi_usb_transmission_result_t res;
    int best_config = 0;

    for (int i = 1; i < (int)dev->dev_desc.b_num_configurations; i++) {
        if (compare_configurations(dev, i, best_config)) {
            best_config = i;
        }
    }

    dev->config = dev->configs[best_config];

    res = control_transfer(dev, &dev->eps[0][0], &(struct cdi_usb_setup_packet){
            .bm_request_type    = CDI_USB_CREQ_DEVICE | CDI_USB_CREQ_OUT,
            .b_request          = CDI_USB_CREQ_SET_CONFIGURATION,
            .w_value            = dev->config->b_configuration_value,
        }, NULL);
    if (res != CDI_USB_OK) {
        dev->config = NULL;
        return -EIO;
    }

    return 0;
}


#ifdef DEBUG
static void debug_device(usb_device_t *dev)
{
    printf("[usb] new %s device %04x:%04x (%x.%x.%x) ",
           dev->speed == CDI_USB_LOW_SPEED  ?  "low speed" :
           dev->speed == CDI_USB_FULL_SPEED ? "full speed" :
           dev->speed == CDI_USB_HIGH_SPEED ? "high speed" :
           dev->speed == CDI_USB_SUPERSPEED ? "superspeed" :
                                              "futurespeed",
           dev->dev_desc.id_vendor, dev->dev_desc.id_product,
           dev->dev_desc.b_device_class,
           dev->dev_desc.b_device_sub_class,
           dev->dev_desc.b_device_protocol);

    struct cdi_usb_string_descriptor str;

    if (dev->dev_desc.i_manufacturer) {
        get_string_descriptor(dev, dev->dev_desc.i_manufacturer, &str);
        for (int i = 0; i < (str.b_length - 2) / 2; i++) {
            putchar(str.b_string[i] > 0x7f ? '?' : str.b_string[i]);
        }
        putchar(' ');
    }

    if (dev->dev_desc.i_product) {
        get_string_descriptor(dev, dev->dev_desc.i_product, &str);
        for (int i = 0; i < (str.b_length - 2) / 2; i++) {
            putchar(str.b_string[i] > 0x7f ? '?' : str.b_string[i]);
        }
        putchar(' ');
    }

    putchar('\n');
}

static void debug_configuration(usb_device_t *dev)
{
    printf("[usb] chose configuration %i ", dev->config->b_configuration_value);

    struct cdi_usb_string_descriptor str;

    if (dev->config->i_configuration) {
        get_string_descriptor(dev, dev->config->i_configuration, &str);
        putchar('(');
        for (int i = 0; i < (str.b_length - 2) / 2; i++) {
            putchar(str.b_string[i] > 0x7f ? '?' : str.b_string[i]);
        }
        putchar(')');
        putchar(' ');
    }

    printf("with %i interface(s):\n", dev->config->b_num_interfaces);

    struct cdi_usb_interface_descriptor *ifc = (void *)(dev->config + 1);
    for (int i = 0; i < (int)dev->config->b_num_interfaces; i++) {
        printf("[usb] ");
        if (ifc->i_interface) {
            get_string_descriptor(dev, ifc->i_interface, &str);
            for (int j = 0; j < (int)dev->config->b_num_interfaces; j++) {
                putchar(str.b_string[j] > 0x7f ? '?' : str.b_string[j]);
            }
            putchar(' ');
        }
        printf("%x.%x.%x\n", ifc->b_interface_class,
               ifc->b_interface_sub_class, ifc->b_interface_protocol);

        ifc = (void *)((struct cdi_usb_endpoint_descriptor *)(ifc + 1)
                + ifc->b_num_endpoints);
    }
}
#endif


static void new_hub_device(usb_logical_device_t *ldev);

static void new_hub(usb_hub_t *hub)
{
    for (int i = 0; i < hub->cdi->ports; i++) {
        hub->drv->port_disable(hub->cdi, i);
    }

    DPRINTF("[usb] new hub with %i ports detected\n", hub->cdi->ports);

    for (int i = 0; i < hub->cdi->ports; i++) {
        hub->drv->port_reset_enable(hub->cdi, i);
        cdi_sleep_ms(5);

        cdi_usb_port_status_t status = hub->drv->port_status(hub->cdi, i);
        if (!(status & CDI_USB_PORT_CONNECTED)) {
            hub->drv->port_disable(hub->cdi, i);
            continue;
        }

        int mps0 = get_mps0(hub, i, status & CDI_USB_PORT_SPEED_MASK);
        if (mps0 < 0) {
            fprintf(stderr, "[usb] failed to inquire MPS0\n");
            hub->drv->port_disable(hub->cdi, i);
            continue;
        }

        hub->drv->port_reset_enable(hub->cdi, i);
        cdi_sleep_ms(5);

        status = hub->drv->port_status(hub->cdi, i);
        if (!(status & CDI_USB_PORT_CONNECTED)) {
            hub->drv->port_disable(hub->cdi, i);
            continue;
        }

        usb_device_t *dev = enumerate(hub, i, status & CDI_USB_PORT_SPEED_MASK,
                                      mps0);
        if (!dev) {
            fprintf(stderr, "[usb] failed to enumerate device\n");
            hub->drv->port_disable(hub->cdi, i);
            continue;
        }

#ifdef DEBUG
        debug_device(dev);
#endif

        usb_logical_device_t *ldev = NULL;
        int ldev_ep_idx = 0;
        bool interface_level;

        interface_level = dev->dev_desc.b_device_class == CDI_USB_DEVCLS_NONE;
        if (!interface_level) {
            ldev = calloc(1, sizeof(*ldev));
            ldev->cdi.bus_data.bus_type = CDI_USB;
            ldev->cdi.interface         = -1;
            ldev->cdi.endpoint_count    = 1; // EP0
            ldev->cdi.vendor_id         = dev->dev_desc.id_vendor;
            ldev->cdi.product_id        = dev->dev_desc.id_product;
            ldev->cdi.class_id          = dev->dev_desc.b_device_class;
            ldev->cdi.subclass_id       = dev->dev_desc.b_device_sub_class;
            ldev->cdi.protocol_id       = dev->dev_desc.b_device_protocol;

            ldev->dev = dev;

            ldev->endpoints = malloc(1 * sizeof(ldev->endpoints[0]));
            ldev->endpoints[0] = 0; // EP0
            ldev_ep_idx = 1;
        }

        if (get_configurations(dev) < 0) {
            fprintf(stderr, "[usb] failed to get device configurations\n");
            free(ldev->endpoints);
            free(ldev);
            free(dev);
            hub->drv->port_disable(hub->cdi, i);
            continue;
        }
        if (choose_configuration(dev) < 0) {
            fprintf(stderr, "[usb] failed to select device configuration\n");
            free(ldev->endpoints);
            free(ldev);
            for (int j = 0; j < (int)dev->dev_desc.b_num_configurations; j++) {
                free(dev->configs[j]);
            }
            free(dev->config);
            free(dev);
            hub->drv->port_disable(hub->cdi, i);
            continue;
        }

#ifdef DEBUG
        debug_configuration(dev);
#endif

        struct cdi_usb_interface_descriptor *ifc = (void *)(dev->config + 1);
        for (int j = 0; j < (int)dev->config->b_num_interfaces; j++) {
            if (interface_level) {
                ldev = calloc(1, sizeof(*ldev));
                ldev->cdi.bus_data.bus_type = CDI_USB;
                ldev->cdi.interface         = j;
                // Include EP0
                ldev->cdi.endpoint_count    = ifc->b_num_endpoints + 1;
                ldev->cdi.vendor_id         = dev->dev_desc.id_vendor;
                ldev->cdi.product_id        = dev->dev_desc.id_product;
                ldev->cdi.class_id          = ifc->b_interface_class;
                ldev->cdi.subclass_id       = ifc->b_interface_sub_class;
                ldev->cdi.protocol_id       = ifc->b_interface_protocol;

                ldev->dev = dev;

                ldev->endpoints = malloc(ldev->cdi.endpoint_count
                                         * sizeof(ldev->endpoints[0]));
                ldev->endpoints[0] = 0; // EP0
                ldev_ep_idx = 1;
            } else {
                ldev->cdi.endpoint_count += ifc->b_num_endpoints;
                ldev->endpoints = realloc(ldev->endpoints,
                                          ldev->cdi.endpoint_count
                                          * sizeof(ldev->endpoints[0]));
            }

            struct cdi_usb_endpoint_descriptor *ep = (void *)(ifc + 1);
            for (int k = 0; k < (int)ifc->b_num_endpoints; k++, ep++) {
                bool in = ep->b_endpoint_address & CDI_USB_EPDSC_EPADR_DIR;
                int id  = CDI_USB_EPDSC_EPADR_ID(ep->b_endpoint_address);

                dev->eps[in][id].ep     = id;
                dev->eps[in][id].mps
                    = CDI_USB_EPDSC_MPS_MPS(ep->w_max_packet_size);
                dev->eps[in][id].desc   = ep;

                switch (ep->bm_attributes & CDI_USB_EPDSC_ATTR_XFER_TYPE_MASK) {
                    case CDI_USB_EPDSC_ATTR_CONTROL:
                        dev->eps[in][id].type = CDI_USB_CONTROL;
                        break;
                    case CDI_USB_EPDSC_ATTR_ISOCHRONOUS:
                        dev->eps[in][id].type = CDI_USB_ISOCHRONOUS;
                        break;
                    case CDI_USB_EPDSC_ATTR_BULK:
                        dev->eps[in][id].type = CDI_USB_BULK;
                        break;
                    case CDI_USB_EPDSC_ATTR_INTERRUPT:
                        dev->eps[in][id].type = CDI_USB_INTERRUPT;
                        break;
                }

                ldev->endpoints[ldev_ep_idx++] = ((int)in << 4) | id;
            }

            if (interface_level) {
                if (ldev->cdi.class_id == CDI_USB_DEVCLS_HUB) {
                    new_hub_device(ldev);
                } else {
                    cdi_provide_device(&ldev->cdi.bus_data);
                }
            }

            ifc = (void *)ep;
        }

        if (!interface_level) {
            if (ldev->cdi.class_id == CDI_USB_DEVCLS_HUB) {
                new_hub_device(ldev);
            } else {
                cdi_provide_device(&ldev->cdi.bus_data);
            }
        }
    }
}

struct cdi_device *usb_init_hc(struct cdi_bus_data *bus_data)
{
    struct cdi_usb_hc_bus *hc_bus = CDI_UPCAST(bus_data, struct cdi_usb_hc_bus,
                                               bus_data);
    struct cdi_usb_hc *hc = hc_bus->hc;
    struct cdi_usb_hcd *hcd = CDI_UPCAST(hc->dev.driver, struct cdi_usb_hcd,
                                         drv);

    usb_hub_t *hub = calloc(1, sizeof(*hub));
    hub->cdi = &hc->rh;
    hub->hc = hc;
    hub->drv = &hcd->rh_drv;

    new_hub(hub);

    return NULL;
}


void usb_get_endpoint_descriptor(struct cdi_usb_device *dev, int ep,
                                 struct cdi_usb_endpoint_descriptor *desc)
{
    usb_logical_device_t *ldev = CDI_UPCAST(dev, usb_logical_device_t, cdi);

    if (ep < 0 || ep >= dev->endpoint_count) {
        return;
    }

    int ep_idx = ldev->endpoints[ep];
    usb_endpoint_t *ep_info = &ldev->dev->eps[ep_idx >> 4][ep_idx & 0xf];

    if (ep_idx) {
        assert(ep_info->desc);
        memcpy(desc, ep_info->desc, sizeof(*desc));
    } else {
        *desc = (struct cdi_usb_endpoint_descriptor){
            .b_length           = sizeof(*desc),
            .b_descriptor_type  = CDI_USB_DESC_ENDPOINT,
            .b_endpoint_address = 0,
            .bm_attributes      = 0x00,
            .w_max_packet_size  = ep_info->mps,
            .b_interval         = 0
        };
    }
}


cdi_usb_transmission_result_t
    usb_control_transfer(struct cdi_usb_device *dev, int ep,
                         const struct cdi_usb_setup_packet *setup, void *data)
{
    usb_logical_device_t *ldev = CDI_UPCAST(dev, usb_logical_device_t, cdi);

    if (ep < 0 || ep >= dev->endpoint_count) {
        return CDI_USB_DRIVER_ERROR;
    }

    int ep_idx = ldev->endpoints[ep];
    usb_endpoint_t *ep_info = &ldev->dev->eps[ep_idx >> 4][ep_idx & 0xf];

    if (ep_info->type != CDI_USB_CONTROL) {
        return CDI_USB_DRIVER_ERROR;
    }

    return control_transfer(ldev->dev, ep_info, setup, data);
}


cdi_usb_transmission_result_t usb_bulk_transfer(struct cdi_usb_device *dev,
                                                int ep, void *data, size_t size)
{
    usb_logical_device_t *ldev = CDI_UPCAST(dev, usb_logical_device_t, cdi);

    if (ep < 0 || ep >= dev->endpoint_count) {
        return CDI_USB_DRIVER_ERROR;
    }

    int ep_idx = ldev->endpoints[ep];
    usb_endpoint_t *ep_info = &ldev->dev->eps[ep_idx >> 4][ep_idx & 0xf];

    if (ep_info->type != CDI_USB_BULK) {
        return CDI_USB_DRIVER_ERROR;
    }

    struct cdi_usb_hcd *hcd = CDI_UPCAST(ldev->dev->hc->dev.driver,
                                         struct cdi_usb_hcd, drv);
    cdi_usb_hc_transaction_t ta;
    cdi_usb_transmission_result_t res;

    ta = hcd->create_transaction(ldev->dev->hc, &(struct cdi_usb_hc_ep_info){
            .dev        = ldev->dev->id,
            .ep         = ep_info->ep,
            .ep_type    = ep_info->type,
            .speed      = ldev->dev->speed,
            .mps        = ep_info->mps,
            .tt_addr    = ldev->dev->tt_addr,
            .tt_port    = ldev->dev->tt_port
        });

    hcd->enqueue(ldev->dev->hc, &(struct cdi_usb_hc_transmission){
            .ta     = ta,
            .token  = ep_idx >> 4 ? CDI_USB_IN : CDI_USB_OUT,
            .buffer = data,
            .size   = size,
            .toggle = ep_info->toggle,
            .result = &res
        });
    ep_info->toggle ^= (((size + ep_info->mps - 1) / ep_info->mps) & 1);

    hcd->wait_transaction(ldev->dev->hc, ta);
    hcd->destroy_transaction(ldev->dev->hc, ta);

    return res;
}


static struct cdi_usb_hub_driver hub_drv;


static void new_hub_device(usb_logical_device_t *ldev)
{
    cdi_usb_transmission_result_t res;
    usb_hub_device_t *hub;
    struct usb_hub_descriptor *hub_desc = NULL;

    assert(ldev->cdi.interface < 0);

    hub = calloc(1, sizeof(*hub));

    hub->hub = calloc(1, sizeof(*hub->hub));
    hub->hub->cdi = &hub->cdi;
    hub->hub->drv = &hub_drv;
    hub->hub->hc = ldev->dev->hc;
    hub->hub->ldev = ldev;

    uint8_t hub_desc_length;
    res = usb_control_transfer(&ldev->cdi, 0, &(struct cdi_usb_setup_packet){
            .bm_request_type    = CDI_USB_CREQ_DEVICE | CDI_USB_CREQ_CLASS
                                | CDI_USB_CREQ_IN,
            .b_request          = CDI_USB_CREQ_GET_DESCRIPTOR,
            .w_value            = (USB_DESC_HUB << 8) | 0,
            .w_length           = 1
        }, &hub_desc_length);
    if (res != CDI_USB_OK ||
        hub_desc_length < sizeof(struct usb_hub_descriptor))
    {
        goto fail;
    }

    hub_desc = malloc(hub_desc_length);
    res = usb_control_transfer(&ldev->cdi, 0, &(struct cdi_usb_setup_packet){
            .bm_request_type    = CDI_USB_CREQ_DEVICE | CDI_USB_CREQ_CLASS
                                | CDI_USB_CREQ_IN,
            .b_request          = CDI_USB_CREQ_GET_DESCRIPTOR,
            .w_value            = (USB_DESC_HUB << 8) | 0,
            .w_length           = hub_desc_length
        }, hub_desc);
    if (res != CDI_USB_OK) {
        goto fail;
    }

    int ports = hub_desc->b_nbr_ports;
    hub->cdi.ports = ports;

    // Enable port power
    if (!(hub_desc->w_hub_characteristics & (1 << 1))) {
        // Hub supports power switching, enable global power
        res = usb_control_transfer(&ldev->cdi, 0,
            &(struct cdi_usb_setup_packet){
                .bm_request_type    = CDI_USB_CREQ_DEVICE
                                    | CDI_USB_CREQ_CLASS | CDI_USB_CREQ_OUT,
                .b_request          = CDI_USB_CREQ_SET_FEATURE,
                .w_value            = C_HUB_LOCAL_POWER
            }, NULL);
        if (res != CDI_USB_OK) {
            goto fail;
        }
    }

    if ((hub_desc->w_hub_characteristics & 0x3) == 1) {
        // Hub supports per-port power switching, enable power for each port
        for (int port = 0; port < ports; port++) {
            res = usb_control_transfer(&ldev->cdi, 0,
                &(struct cdi_usb_setup_packet){
                    .bm_request_type    = CDI_USB_CREQ_OTHER
                                        | CDI_USB_CREQ_CLASS
                                        | CDI_USB_CREQ_OUT,
                    .b_request          = CDI_USB_CREQ_SET_FEATURE,
                    .w_value            = PORT_POWER,
                    .w_index            = port + 1
                }, NULL);
            if (res != CDI_USB_OK) {
                goto fail;
            }
        }
    }

    cdi_sleep_ms(hub_desc->b_pwr_on_2_pwr_good * 2);

    free(hub_desc);

    new_hub(hub->hub);

    return;

fail:
    free(hub_desc);
    free(hub->hub);
    free(hub);
    return;
}


static void hub_port_down(struct cdi_usb_hub *cdi_hub, int index)
{
    usb_hub_device_t *hub = CDI_UPCAST(cdi_hub, usb_hub_device_t, cdi);

    usb_control_transfer(&hub->hub->ldev->cdi, 0,
        &(struct cdi_usb_setup_packet){
            .bm_request_type    = CDI_USB_CREQ_OTHER | CDI_USB_CREQ_CLASS
                                | CDI_USB_CREQ_OUT,
            .b_request          = CDI_USB_CREQ_CLEAR_FEATURE,
            .w_value            = PORT_ENABLE,
            .w_index            = index + 1
        }, NULL);
}


static void hub_port_up(struct cdi_usb_hub *cdi_hub, int index)
{
    usb_hub_device_t *hub = CDI_UPCAST(cdi_hub, usb_hub_device_t, cdi);

    usb_control_transfer(&hub->hub->ldev->cdi, 0,
        &(struct cdi_usb_setup_packet){
            .bm_request_type    = CDI_USB_CREQ_OTHER | CDI_USB_CREQ_CLASS
                                | CDI_USB_CREQ_OUT,
            .b_request          = CDI_USB_CREQ_SET_FEATURE,
            .w_value            = PORT_ENABLE,
            .w_index            = index + 1
        }, NULL);

    usb_control_transfer(&hub->hub->ldev->cdi, 0,
        &(struct cdi_usb_setup_packet){
            .bm_request_type    = CDI_USB_CREQ_OTHER | CDI_USB_CREQ_CLASS
                                | CDI_USB_CREQ_OUT,
            .b_request          = CDI_USB_CREQ_SET_FEATURE,
            .w_value            = PORT_RESET,
            .w_index            = index + 1
        }, NULL);

    cdi_sleep_ms(50);

    usb_control_transfer(&hub->hub->ldev->cdi, 0,
        &(struct cdi_usb_setup_packet){
            .bm_request_type    = CDI_USB_CREQ_OTHER | CDI_USB_CREQ_CLASS
                                | CDI_USB_CREQ_OUT,
            .b_request          = CDI_USB_CREQ_CLEAR_FEATURE,
            .w_value            = PORT_RESET,
            .w_index            = index + 1
        }, NULL);

    cdi_sleep_ms(5);
}


static cdi_usb_port_status_t hub_port_status(struct cdi_usb_hub *cdi_hub,
                                             int index)
{
    cdi_usb_transmission_result_t res;
    usb_hub_device_t *hub = CDI_UPCAST(cdi_hub, usb_hub_device_t, cdi);

    uint32_t portsc;
    res = usb_control_transfer(&hub->hub->ldev->cdi, 0,
        &(struct cdi_usb_setup_packet){
            .bm_request_type    = CDI_USB_CREQ_OTHER | CDI_USB_CREQ_CLASS
                                | CDI_USB_CREQ_IN,
            .b_request          = CDI_USB_CREQ_GET_STATUS,
            .w_index            = index + 1,
            .w_length           = sizeof(portsc)
        }, &portsc);
    if (res != CDI_USB_OK) {
        return 0;
    }

    cdi_usb_port_status_t status = 0;

    if (portsc & ((1 << PORT_ENABLE) | (1 << PORT_CONNECTION))) {
        status |= CDI_USB_PORT_CONNECTED;
        if (portsc & (1 << PORT_LOW_SPEED)) {
            status |= CDI_USB_LOW_SPEED;
        } else if (portsc & (1 << PORT_HIGH_SPEED)) {
            status |= CDI_USB_HIGH_SPEED;
        } else {
            status |= CDI_USB_FULL_SPEED;
        }
    }

    return status;
}


static struct cdi_usb_hub_driver hub_drv = {
    .port_disable       = hub_port_down,
    .port_reset_enable  = hub_port_up,
    .port_status        = hub_port_status,
};
