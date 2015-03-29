#include <assert.h>
#include <drivers.h>
#include <ipc.h>
#include <shm.h>
#include <unistd.h>
#include <usb-ipc.h>
#include <cdi.h>
#include <cdi/misc.h>
#include <cdi/usb.h>


extern void cdi_osdep_new_device(struct cdi_driver *drv, struct cdi_device *dev);

static struct cdi_driver *usb_dd;

struct ipc_usb_device {
    struct cdi_usb_device cdi;
    pid_t pid;
    int id;
};


static uintmax_t usb_ipc_device_found(void)
{
    struct usb_ipc_device_found_param par;
    popup_get_message(&par, sizeof(par));

    struct ipc_usb_device *dev = calloc(1, sizeof(*dev));
    dev->cdi = par.dev;
    dev->pid = par.drv_pid;
    dev->id  = par.id;

    cdi_osdep_new_device(usb_dd, usb_dd->init_device(&dev->cdi.bus_data));

    return 0;
}


void cdi_osdep_set_up_usb_dd_ipc(struct cdi_driver *drv);

void cdi_osdep_set_up_usb_dd_ipc(struct cdi_driver *drv)
{
    assert(!usb_dd);
    usb_dd = drv;

    popup_message_handler(USB_IPC_DEVICE_FOUND, usb_ipc_device_found);
}


void cdi_osdep_handle_usb_device(struct cdi_usb_bus_device_pattern *p);

void cdi_osdep_handle_usb_device(struct cdi_usb_bus_device_pattern *p)
{
    char alias[32];

    if (p->vendor_id < 0 && p->product_id < 0) {
        assert(p->class_id >= 0);
        if (p->protocol_id < 0) {
            assert(p->subclass_id < 0);
            snprintf(alias, 32, "usb-%02x-?-?", p->class_id);
        } else if (p->subclass_id < 0) {
            snprintf(alias, 32, "usb-%02x-%02x-?", p->class_id, p->subclass_id);
        } else {
            snprintf(alias, 32, "usb-%02x-%02x-%02x", p->class_id, p->subclass_id, p->protocol_id);
        }
    } else if (p->class_id < 0 && p->subclass_id < 0 && p->protocol_id < 0) {
        assert(p->vendor_id >= 0 && p->product_id >= 0);
        snprintf(alias, 32, "usb-%04x-%04x", p->vendor_id, p->product_id);
    } else {
        assert(0);
    }

    process_add_alias(alias);
}


void cdi_usb_get_endpoint_descriptor(struct cdi_usb_device *dev, int ep, struct cdi_usb_endpoint_descriptor *desc)
{
    struct ipc_usb_device *ipc_dev = CDI_UPCAST(dev, struct ipc_usb_device, cdi);
    uintptr_t reply;

    struct usb_ipc_get_endpoint_descriptor_param par = {
        .dev   = ipc_dev->id,
        .ep    = ep
    };

    ipc_message_request(ipc_dev->pid, USB_IPC_GET_ENDPOINT_DESCRIPTOR, &par, sizeof(par), &reply);
    popup_get_answer(reply, desc, sizeof(*desc));
}


cdi_usb_transmission_result_t cdi_usb_control_transfer(struct cdi_usb_device *dev, int ep,
                                                       const struct cdi_usb_setup_packet *setup, void *data)
{
    struct ipc_usb_device *ipc_dev = CDI_UPCAST(dev, struct ipc_usb_device, cdi);
    off_t offset = (uintptr_t)data % PAGE_SIZE;
    int page_count = (offset + setup->w_length + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t shm = shm_make(1, &data, &page_count, offset);

    struct usb_ipc_control_transfer_param par = {
        .dev   = ipc_dev->id,
        .ep    = ep,
        .setup = *setup
    };

    cdi_usb_transmission_result_t res;
    errno = 0;
    res = ipc_shm_message_synced(ipc_dev->pid, USB_IPC_CONTROL_TRANSFER, shm, &par, sizeof(par));
    shm_unmake(shm, true);
    return errno ? CDI_USB_DRIVER_ERROR : res;
}


cdi_usb_transmission_result_t cdi_usb_bulk_transfer(struct cdi_usb_device *dev, int ep, void *data, size_t size)
{
    struct ipc_usb_device *ipc_dev = CDI_UPCAST(dev, struct ipc_usb_device, cdi);
    off_t offset = (uintptr_t)data % PAGE_SIZE;
    int page_count = (offset + size + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t shm = shm_make(1, &data, &page_count, offset);

    struct usb_ipc_bulk_transfer_param par = {
        .dev   = ipc_dev->id,
        .ep    = ep,
        .size  = size
    };

    cdi_usb_transmission_result_t res;
    errno = 0;
    res = ipc_shm_message_synced(ipc_dev->pid, USB_IPC_BULK_TRANSFER, shm, &par, sizeof(par));
    shm_unmake(shm, true);
    return errno ? CDI_USB_DRIVER_ERROR : res;
}
