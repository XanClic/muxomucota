#include <ipc.h>
#include <stdint.h>
#include <stdlib.h>
#include <syscall.h>
#include <cdi.h>
#include <cdi/lists.h>
#include <cdi/misc.h>

#ifdef X86
#include <drivers/ports.h>
#endif


struct irq_info
{
    struct cdi_device *dev;
    void (*handler)(struct cdi_device *);
};


static void raw_irq_handler(void *info)
{
    struct irq_info *irq_info = info;

    irq_info->handler(irq_info->dev);

    irq_handler_exit();
}


void cdi_register_irq(uint8_t irq, void (*handler)(struct cdi_device *), struct cdi_device *device)
{
    struct irq_info *info = malloc(sizeof(*info));

    info->dev = device;
    info->handler = handler;

    register_irq_handler(irq, &raw_irq_handler, info);
}


void cdi_sleep_ms(uint32_t ms)
{
    syscall1(SYS_SLEEP, ms);
}


#ifdef X86
int cdi_ioports_alloc(uint16_t start, uint16_t count)
{
    (void)start;
    (void)count;

    iopl(3); // kinda FIXME

    return 0;
}


int cdi_ioports_free(uint16_t start, uint16_t count)
{
    (void)start;
    (void)count;

    return 0;
}
#endif
