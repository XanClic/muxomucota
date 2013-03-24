#include <assert.h>
#include <compiler.h>
#include <ipc.h>
#include <shm.h>
#include <unistd.h>
#include <vfs.h>
#include <cdi.h>
#include <cdi/lists.h>
#include <cdi/net.h>


extern void cdi_osdep_device_found(void);


static struct cdi_net_driver *net_driver;


static uintmax_t send_packet(uintptr_t shmid);

void cdi_net_driver_register(struct cdi_net_driver *driver);


void cdi_net_driver_init(struct cdi_net_driver *driver)
{
    driver->drv.type = CDI_NETWORK;
    cdi_driver_init((struct cdi_driver *)driver);
}


void cdi_net_driver_register(struct cdi_net_driver *driver)
{
    net_driver = driver;

    popup_shm_handler(STREAM_SEND, send_packet);
}


void cdi_net_driver_destroy(struct cdi_net_driver* driver)
{
    cdi_driver_destroy((struct cdi_driver *)driver);
}


void cdi_net_device_init(struct cdi_net_device *device)
{
    static int number = 0;
    device->number = number++;

    int fd = create_pipe("(eth)/register", O_WRONLY);

    assert(fd >= 0);

    struct
    {
        uintptr_t id;
        uint8_t mac[6];
    } cc_packed reg_info = {
        .id = (uintptr_t)device
    };

    uint64_t mac = device->mac;

    for (int i = 0; i < 6; i++, mac >>= 8)
        reg_info.mac[i] = mac & 0xff;

    stream_send(fd, &reg_info, sizeof(reg_info), O_BLOCKING);

    destroy_pipe(fd, 0);


    cdi_osdep_device_found();
}


void cdi_net_receive(struct cdi_net_device *device, void *buffer, size_t size)
{
    int fd = create_pipe("(eth)/receive", O_WRONLY);

    assert(fd >= 0);

    pipe_set_flag(fd, F_DEVICE_ID, (uintptr_t)device);
    stream_send(fd, buffer, size, O_NONBLOCK);

    destroy_pipe(fd, 0);
}


static uintmax_t send_packet(uintptr_t shmid)
{
    struct cdi_net_device *device;

    size_t recv = popup_get_message(&device, sizeof(device));
    assert(recv == sizeof(device));

    const void *data = shm_open(shmid, VMM_UR);

    size_t size = shm_size(shmid);

    // Eigentlich sollte der Treiber nicht schreibend auf die Daten zugreifen.
    // Tut ers doch, gibts einen Pagefault. Nur gerecht.
    net_driver->send_packet(device, (void *)data, size);

    return size;
}
