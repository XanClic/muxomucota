#ifndef _USB_IPC_H
#define _USB_IPC_H

#include <vfs.h>
#include <cdi/usb.h>
#include <cdi/usb_hcd.h>


// Provided by HCDs
enum {
    USB_IPC_HC_CREATE_TRANSACTION,
    USB_IPC_HC_ENQUEUE,
    USB_IPC_HC_WAIT_TRANSACTION,
    USB_IPC_HC_DESTROY_TRANSACTION,

    USB_IPC_RH_PORT_DISABLE,
    USB_IPC_RH_PORT_RESET_ENABLE,
    USB_IPC_RH_PORT_STATUS,
};


struct usb_ipc_hc_create_transaction_param {
    int hc;
    struct cdi_usb_hc_ep_info target;
};

struct usb_ipc_hc_enqueue_param {
    int hc;
    // @buffer and @result are offsets into the SHM provided
    struct cdi_usb_hc_transmission trans;
};

struct usb_ipc_hc_wait_transaction_param {
    int hc;
    cdi_usb_hc_transaction_t ta;
};

struct usb_ipc_hc_destroy_transaction_param {
    int hc;
    cdi_usb_hc_transaction_t ta;
};

struct usb_ipc_hc_port_param {
    int hc, index;
};


// Provided by the bus driver
enum {
    USB_IPC_HC_FOUND,

    USB_IPC_GET_ENDPOINT_DESCRIPTOR,

    USB_IPC_CONTROL_TRANSFER,
    USB_IPC_BULK_TRANSFER,
};

struct usb_ipc_hc_found_param {
    pid_t drv_pid;
    char drv_name[32], hc_name[32];
    int id;
    struct cdi_usb_hub rh;
};

struct usb_ipc_get_endpoint_descriptor_param {
    int dev, ep;
};

struct usb_ipc_control_transfer_param {
    int dev, ep;
    struct cdi_usb_setup_packet setup;
    // @buffer is in the SHM provided
};

struct usb_ipc_bulk_transfer_param {
    int dev, ep;
    size_t size;
    // @buffer is in the SHM provided
};


// Provided by device drivers
enum {
    USB_IPC_DEVICE_FOUND = FIRST_NON_VFS_IPC_FUNC,
};

struct usb_ipc_device_found_param {
    struct cdi_usb_device dev;
    pid_t drv_pid;
    int id;
};

#endif
