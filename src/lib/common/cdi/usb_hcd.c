#include <errno.h>
#include <ipc.h>
#include <shm.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <usb-ipc.h>
#include <cdi/lists.h>
#include <cdi/misc.h>
#include <cdi/usb_hcd.h>


extern void cdi_osdep_device_found(void);


struct transmission {
    uintptr_t shmid;
    void *mapping;
};

struct transaction {
    cdi_usb_hc_transaction_t ta;
    cdi_list_t transmissions;
};

struct usb_hc {
    struct cdi_usb_hc *cdi;

    struct transaction **transactions;
    size_t transactions_size;
    size_t transactions_first_free;
};

static struct usb_hc **hc_list;
static size_t hc_list_size, hc_list_elements;


static uintmax_t usb_ipc_hc_create_transaction(void)
{
    struct usb_ipc_hc_create_transaction_param par;
    popup_get_message(&par, sizeof(par));
    if (par.hc < 0 || par.hc > (int)hc_list_elements || !hc_list[par.hc]) {
        errno = ENOENT;
        return 0;
    }

    struct usb_hc *hc = hc_list[par.hc];
    struct cdi_usb_hcd *hcd = CDI_UPCAST(hc->cdi->dev.driver, struct cdi_usb_hcd, drv);
    struct transaction *ta = malloc(sizeof(*ta));

    *ta = (struct transaction){
        .ta             = hcd->create_transaction(hc->cdi, &par.target),
        .transmissions  = cdi_list_create()
    };

    size_t idx;
    for (idx = hc->transactions_first_free; idx < hc->transactions_size && hc->transactions[idx]; idx++);
    if (idx >= hc->transactions_size) {
        size_t old_ts = hc->transactions_size;
        hc->transactions_size = (hc->transactions_size + 4) * 2 /3;
        hc->transactions = realloc(hc->transactions, hc->transactions_size * sizeof(*hc->transactions));
        memset(hc->transactions + old_ts, 0, (hc->transactions_size - old_ts) * sizeof(*hc->transactions));
    }
    hc->transactions[idx] = ta;
    hc->transactions_first_free = idx + 1;

    return idx;
}


static uintmax_t usb_ipc_hc_enqueue(uintptr_t shm)
{
    struct usb_ipc_hc_enqueue_param par;
    popup_get_message(&par, sizeof(par));
    if (par.hc < 0 || par.hc > (int)hc_list_elements || !hc_list[par.hc]) {
        errno = ENOENT;
        return 0;
    }

    struct usb_hc *hc = hc_list[par.hc];
    struct cdi_usb_hcd *hcd = CDI_UPCAST(hc->cdi->dev.driver, struct cdi_usb_hcd, drv);

    if ((uintptr_t)par.trans.ta > hc->transactions_size || !hc->transactions[(uintptr_t)par.trans.ta]) {
        errno = ENOENT;
        return 0;
    }

    struct transaction *ta = hc->transactions[(uintptr_t)par.trans.ta];

    size_t sz = shm_size(shm);
    if ((uintptr_t)par.trans.buffer >= sz ||
        (uintptr_t)par.trans.buffer + par.trans.size > sz ||
        (uintptr_t)par.trans.result >= sz ||
        (uintptr_t)par.trans.result + sizeof(*par.trans.result) > sz)
    {
        errno = ENOENT;
        return 0;
    }

    struct transmission *tm = malloc(sizeof(*tm));
    *tm = (struct transmission) {
        .shmid      = shm,
        .mapping    = shm_open(shm, VMM_UR | VMM_UW)
    };

    par.trans.ta = ta->ta;
    par.trans.buffer = (void *)((uintptr_t)tm->mapping + (uintptr_t)par.trans.buffer);
    par.trans.result = (void *)((uintptr_t)tm->mapping + (uintptr_t)par.trans.result);

    hcd->enqueue(hc->cdi, &par.trans);

    cdi_list_push(ta->transmissions, tm);

    return 0;
}


static uintmax_t usb_ipc_hc_wait_transaction(void)
{
    struct usb_ipc_hc_wait_transaction_param par;
    popup_get_message(&par, sizeof(par));
    if (par.hc < 0 || par.hc > (int)hc_list_elements || !hc_list[par.hc]) {
        errno = ENOENT;
        return 0;
    }

    struct usb_hc *hc = hc_list[par.hc];
    struct cdi_usb_hcd *hcd = CDI_UPCAST(hc->cdi->dev.driver, struct cdi_usb_hcd, drv);

    if ((uintptr_t)par.ta > hc->transactions_size || !hc->transactions[(uintptr_t)par.ta]) {
        errno = ENOENT;
        return 0;
    }

    struct transaction *ta = hc->transactions[(uintptr_t)par.ta];

    hcd->wait_transaction(hc->cdi, ta->ta);

    struct transmission *tm;
    while ((tm = cdi_list_pop(ta->transmissions))) {
        shm_close(tm->shmid, tm->mapping, true);
        free(tm);
    }

    return 0;
}


static uintmax_t usb_ipc_hc_destroy_transaction(void)
{
    struct usb_ipc_hc_wait_transaction_param par;
    popup_get_message(&par, sizeof(par));
    if (par.hc < 0 || par.hc > (int)hc_list_elements || !hc_list[par.hc]) {
        errno = ENOENT;
        return 0;
    }

    struct usb_hc *hc = hc_list[par.hc];
    struct cdi_usb_hcd *hcd = CDI_UPCAST(hc->cdi->dev.driver, struct cdi_usb_hcd, drv);

    if ((uintptr_t)par.ta > hc->transactions_size || !hc->transactions[(uintptr_t)par.ta]) {
        errno = ENOENT;
        return 0;
    }

    struct transaction *ta = hc->transactions[(uintptr_t)par.ta];

    hcd->destroy_transaction(hc->cdi, ta->ta);

    hc->transactions[(uintptr_t)par.ta] = NULL;
    hc->transactions_first_free = (uintptr_t)par.ta;

    struct transmission *tm;
    while ((tm = cdi_list_pop(ta->transmissions))) {
        shm_close(tm->shmid, tm->mapping, true);
        free(tm);
    }

    cdi_list_destroy(ta->transmissions);
    free(ta);

    return 0;
}


static uintmax_t usb_ipc_rh_port_disable(void)
{
    struct usb_ipc_hc_port_param par;
    popup_get_message(&par, sizeof(par));
    if (par.hc < 0 || par.hc > (int)hc_list_elements || !hc_list[par.hc]) {
        errno = ENOENT;
        return 0;
    }

    struct cdi_usb_hc *hc = hc_list[par.hc]->cdi;
    struct cdi_usb_hcd *hcd = CDI_UPCAST(hc->dev.driver, struct cdi_usb_hcd, drv);

    hcd->rh_drv.port_disable(&hc->rh, par.index);

    return 0;
}


static uintmax_t usb_ipc_rh_port_reset_enable(void)
{
    struct usb_ipc_hc_port_param par;
    popup_get_message(&par, sizeof(par));
    if (par.hc < 0 || par.hc > (int)hc_list_elements || !hc_list[par.hc]) {
        errno = ENOENT;
        return 0;
    }

    struct cdi_usb_hc *hc = hc_list[par.hc]->cdi;
    struct cdi_usb_hcd *hcd = CDI_UPCAST(hc->dev.driver, struct cdi_usb_hcd, drv);

    hcd->rh_drv.port_reset_enable(&hc->rh, par.index);

    return 0;
}


static uintmax_t usb_ipc_rh_port_status(void)
{
    struct usb_ipc_hc_port_param par;
    popup_get_message(&par, sizeof(par));
    if (par.hc < 0 || par.hc > (int)hc_list_elements || !hc_list[par.hc]) {
        errno = ENOENT;
        return 0;
    }

    struct cdi_usb_hc *hc = hc_list[par.hc]->cdi;
    struct cdi_usb_hcd *hcd = CDI_UPCAST(hc->dev.driver, struct cdi_usb_hcd, drv);

    return hcd->rh_drv.port_status(&hc->rh, par.index);
}


static void set_up_hc_ipc(void)
{
    static bool ipc_set_up;

    if (ipc_set_up) {
        return;
    }
    ipc_set_up = true;

    popup_message_handler(USB_IPC_HC_CREATE_TRANSACTION, usb_ipc_hc_create_transaction);
    popup_shm_handler(USB_IPC_HC_ENQUEUE, usb_ipc_hc_enqueue);
    popup_message_handler(USB_IPC_HC_WAIT_TRANSACTION, usb_ipc_hc_wait_transaction);
    popup_message_handler(USB_IPC_HC_DESTROY_TRANSACTION, usb_ipc_hc_destroy_transaction);

    popup_message_handler(USB_IPC_RH_PORT_DISABLE, usb_ipc_rh_port_disable);
    popup_message_handler(USB_IPC_RH_PORT_RESET_ENABLE, usb_ipc_rh_port_reset_enable);
    popup_message_handler(USB_IPC_RH_PORT_STATUS, usb_ipc_rh_port_status);
}


void cdi_osdep_provide_hc(struct cdi_usb_hc *cdi_hc);

void cdi_osdep_provide_hc(struct cdi_usb_hc *cdi_hc)
{
    struct usb_hc *hc = calloc(1, sizeof(*hc));
    hc->cdi = cdi_hc;

    cdi_osdep_device_found();

    set_up_hc_ipc();

    if (hc_list_size == hc_list_elements) {
        size_t old_hls = hc_list_size;
        hc_list_size = (hc_list_size + 4) * 2 / 3;
        hc_list = realloc(hc_list, hc_list_size * sizeof(*hc_list));
        memset(hc_list + old_hls, 0, (hc_list_size - old_hls) * sizeof(*hc_list));
    }

    int id = hc_list_elements;
    hc_list[hc_list_elements++] = hc;

    pid_t pid = find_daemon_by_name("usb");
    if (pid >= 0) {
        struct usb_ipc_hc_found_param par;

        par.drv_pid = getpgid(0);

        strncpy(par.drv_name, cdi_hc->dev.driver->name, 31);
        par.drv_name[31] = 0;

        strncpy(par.hc_name, cdi_hc->dev.name, 31);
        par.hc_name[31] = 0;

        par.id = id;
        par.rh = cdi_hc->rh;

        ipc_message(pid, USB_IPC_HC_FOUND, &par, sizeof(par));
    }
}
