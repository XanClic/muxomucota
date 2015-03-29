#include <assert.h>
#include <errno.h>
#include <ipc.h>
#include <shm.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <usb-ipc.h>
#include <vmem.h>

#include <cdi.h>
#include <cdi/misc.h>
#include <cdi/usb.h>
#include <cdi/usb_hcd.h>


extern void cdi_osdep_new_device(struct cdi_driver *drv, struct cdi_device *dev);

struct ipc_usb_hc {
    struct cdi_usb_hc cdi;
    pid_t pid;
    int id;
};


static void rh_port_down(struct cdi_usb_hub *hub, int index)
{
    struct cdi_usb_hc *hc = CDI_UPCAST(hub, struct cdi_usb_hc, rh);
    struct ipc_usb_hc *ipc_hc = CDI_UPCAST(hc, struct ipc_usb_hc, cdi);
    struct usb_ipc_hc_port_param par = {
        .hc = ipc_hc->id,
        .index = index
    };

    ipc_message_synced(ipc_hc->pid, USB_IPC_RH_PORT_DISABLE, &par, sizeof(par));
}


static void rh_port_up(struct cdi_usb_hub *hub, int index)
{
    struct cdi_usb_hc *hc = CDI_UPCAST(hub, struct cdi_usb_hc, rh);
    struct ipc_usb_hc *ipc_hc = CDI_UPCAST(hc, struct ipc_usb_hc, cdi);
    struct usb_ipc_hc_port_param par = {
        .hc = ipc_hc->id,
        .index = index
    };

    ipc_message_synced(ipc_hc->pid, USB_IPC_RH_PORT_RESET_ENABLE, &par, sizeof(par));
}


static cdi_usb_port_status_t rh_port_status(struct cdi_usb_hub *hub, int index)
{
    struct cdi_usb_hc *hc = CDI_UPCAST(hub, struct cdi_usb_hc, rh);
    struct ipc_usb_hc *ipc_hc = CDI_UPCAST(hc, struct ipc_usb_hc, cdi);
    struct usb_ipc_hc_port_param par = {
        .hc = ipc_hc->id,
        .index = index
    };

    return ipc_message_synced(ipc_hc->pid, USB_IPC_RH_PORT_STATUS, &par, sizeof(par));
}


static cdi_usb_hc_transaction_t hc_create_transaction(struct cdi_usb_hc *hc, const struct cdi_usb_hc_ep_info *target)
{
    struct ipc_usb_hc *ipc_hc = CDI_UPCAST(hc, struct ipc_usb_hc, cdi);
    struct usb_ipc_hc_create_transaction_param par = {
        .hc = ipc_hc->id,
        .target = *target
    };

    return (cdi_usb_hc_transaction_t)(uintptr_t)ipc_message_synced(ipc_hc->pid, USB_IPC_HC_CREATE_TRANSACTION, &par, sizeof(par));
}


static void hc_enqueue(struct cdi_usb_hc *hc, const struct cdi_usb_hc_transmission *trans)
{
    static cdi_usb_transmission_result_t null_result;
    struct ipc_usb_hc *ipc_hc = CDI_UPCAST(hc, struct ipc_usb_hc, cdi);
    struct usb_ipc_hc_enqueue_param par = {
        .hc = ipc_hc->id,
        .trans = *trans
    };
    if (!par.trans.result) {
        par.trans.result = &null_result;
    }
    int buffer_pages = ((uintptr_t)trans->buffer % PAGE_SIZE + trans->size + PAGE_SIZE - 1) / PAGE_SIZE;
    int result_pages = ((uintptr_t)trans->result % PAGE_SIZE + sizeof(*trans->result) + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t shm;

    if (trans->buffer) {
        shm = shm_make(2, (void *[]){ trans->buffer, par.trans.result }, (int[]){ buffer_pages, result_pages }, 0);
    } else {
        shm = shm_make(1, (void *[]){ par.trans.result }, (int[]){ result_pages }, 0);
        assert(buffer_pages == 0);
    }

    par.trans.buffer = (void *)((uintptr_t)trans->buffer % PAGE_SIZE);
    par.trans.result = (void *)((uintptr_t)par.trans.result % PAGE_SIZE + (uintptr_t)buffer_pages * PAGE_SIZE);

    ipc_shm_message_synced(ipc_hc->pid, USB_IPC_HC_ENQUEUE, shm, &par, sizeof(par));

    shm_unmake(shm, true);
}


static void hc_wait_transaction(struct cdi_usb_hc *hc, cdi_usb_hc_transaction_t ta)
{
    struct ipc_usb_hc *ipc_hc = CDI_UPCAST(hc, struct ipc_usb_hc, cdi);
    struct usb_ipc_hc_wait_transaction_param par = {
        .hc = ipc_hc->id,
        .ta = ta
    };

    ipc_message_synced(ipc_hc->pid, USB_IPC_HC_WAIT_TRANSACTION, &par, sizeof(par));
}


static void hc_destroy_transaction(struct cdi_usb_hc *hc, cdi_usb_hc_transaction_t ta)
{
    struct ipc_usb_hc *ipc_hc = CDI_UPCAST(hc, struct ipc_usb_hc, cdi);
    struct usb_ipc_hc_destroy_transaction_param par = {
        .hc = ipc_hc->id,
        .ta = ta
    };

    ipc_message_synced(ipc_hc->pid, USB_IPC_HC_DESTROY_TRANSACTION, &par, sizeof(par));
}


static struct cdi_usb_hcd wrapper_hcd = {
    .drv = {
        .name   = "ipc-hci",
        .type   = CDI_USB_HCD,
    },

    .rh_drv = {
        .port_disable       = rh_port_down,
        .port_reset_enable  = rh_port_up,
        .port_status        = rh_port_status,
    },

    .create_transaction     = hc_create_transaction,
    .enqueue                = hc_enqueue,
    .wait_transaction       = hc_wait_transaction,
    .destroy_transaction    = hc_destroy_transaction,
};


static struct cdi_usb_device **dev_list;
static size_t dev_list_size;
static struct cdi_usb_driver *usb_drv;


void cdi_osdep_provide_usb_device_to(struct cdi_usb_device *usb_dev, pid_t pid);

void cdi_osdep_provide_usb_device_to(struct cdi_usb_device *usb_dev, pid_t pid)
{
    int idx;
    for (idx = 0; idx < (int)dev_list_size && dev_list[idx]; idx++);

    if (idx == (int)dev_list_size) {
        dev_list_size = (dev_list_size + 4) * 3 / 2;
        dev_list = realloc(dev_list, dev_list_size * sizeof(*dev_list));
        memset(dev_list + idx, 0, (dev_list_size - idx) * sizeof(*dev_list));
    }

    dev_list[idx] = usb_dev;

    struct usb_ipc_device_found_param par = {
        .dev     = *usb_dev,
        .drv_pid = getpgid(0),
        .id      = idx
    };
    ipc_message(pid, USB_IPC_DEVICE_FOUND, &par, sizeof(par));
}


static uintmax_t usb_ipc_hc_found(void)
{
    struct usb_ipc_hc_found_param par;
    popup_get_message(&par, sizeof(par));

    struct cdi_usb_hcd *hcd = malloc(sizeof(*hcd));
    *hcd = wrapper_hcd;
    hcd->drv.name = strdup(par.drv_name);

    struct ipc_usb_hc *hc = calloc(1, sizeof(*hc));
    hc->pid = par.drv_pid;
    hc->id = par.id;
    hc->cdi.dev.driver = &hcd->drv;
    hc->cdi.dev.name = strdup(par.hc_name);
    hc->cdi.rh = par.rh;

    struct cdi_usb_hc_bus *hcb = calloc(1, sizeof(*hcb));
    hcb->bus_data.bus_type = CDI_USB_HCD;
    hcb->hc = &hc->cdi;

    cdi_osdep_new_device(&usb_drv->drv, usb_drv->drv.init_device(&hcb->bus_data));

    return 0;
}


static uintmax_t usb_ipc_get_endpoint_descriptor(void)
{
    struct usb_ipc_get_endpoint_descriptor_param par;
    popup_get_message(&par, sizeof(par));
    if (par.dev < 0 || par.dev >= (int)dev_list_size || !dev_list[par.dev]) {
        errno = ENOENT;
        return 0;
    }

    struct cdi_usb_device *dev = dev_list[par.dev];
    struct cdi_usb_endpoint_descriptor desc;

    usb_drv->get_endpoint_descriptor(dev, par.ep, &desc);
    popup_set_answer(&desc, sizeof(desc));

    return 0;
}


static uintmax_t usb_ipc_control_transfer(uintptr_t shm)
{
    struct usb_ipc_control_transfer_param par;
    popup_get_message(&par, sizeof(par));
    if (par.dev < 0 || par.dev >= (int)dev_list_size || !dev_list[par.dev]) {
        errno = ENOENT;
        return 0;
    }

    struct cdi_usb_device *dev = dev_list[par.dev];
    void *mapping = shm_open(shm, VMM_UR | VMM_UW);

    cdi_usb_transmission_result_t res;
    res = usb_drv->control_transfer(dev, par.ep, &par.setup, mapping);

    shm_close(shm, mapping, true);

    return res;
}


static uintmax_t usb_ipc_bulk_transfer(uintptr_t shm)
{
    struct usb_ipc_bulk_transfer_param par;
    popup_get_message(&par, sizeof(par));
    if (par.dev < 0 || par.dev >= (int)dev_list_size || !dev_list[par.dev]) {
        errno = ENOENT;
        return 0;
    }

    if (par.size > shm_size(shm)) {
        errno = EFAULT;
        return 0;
    }

    struct cdi_usb_device *dev = dev_list[par.dev];
    void *mapping = shm_open(shm, VMM_UR | VMM_UW);

    cdi_usb_transmission_result_t res;
    res = usb_drv->bulk_transfer(dev, par.ep, mapping, par.size);

    shm_close(shm, mapping, true);

    return res;
}


void cdi_usb_driver_register(struct cdi_usb_driver *drv)
{
    assert(!usb_drv);
    usb_drv = drv;

    popup_message_handler(USB_IPC_HC_FOUND, usb_ipc_hc_found);

    popup_message_handler(USB_IPC_GET_ENDPOINT_DESCRIPTOR, usb_ipc_get_endpoint_descriptor);

    popup_shm_handler(USB_IPC_CONTROL_TRANSFER, usb_ipc_control_transfer);
    popup_shm_handler(USB_IPC_BULK_TRANSFER, usb_ipc_bulk_transfer);
}
