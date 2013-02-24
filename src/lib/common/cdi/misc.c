#include <assert.h>
#include <drivers.h>
#include <ipc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include <cdi.h>
#include <cdi/lists.h>
#include <cdi/misc.h>

#ifdef X86
#include <drivers/ports.h>
#endif


#define CDI_IRQ_STACK_SIZE 16384


static volatile long irqs_occured = 0;


struct irq_info
{
    int irq;
    struct cdi_device *dev;
    void (*handler)(struct cdi_device *);
};


static void raw_irq_handler(void *info)
{
    struct irq_info *irq_info = info;

    irqs_occured |= 1 << irq_info->irq;

    irq_info->handler(irq_info->dev);

    irq_handler_exit();
}


static volatile int registered = 0;


static void irq_thread(void *arg)
{
    struct irq_info *ii = arg;

    register_irq_handler(ii->irq, raw_irq_handler, ii);

    daemonize("cdi-irq", false);
}


void cdi_register_irq(uint8_t irq, void (*handler)(struct cdi_device *), struct cdi_device *device)
{
    struct irq_info *info = malloc(sizeof(*info));

    info->irq = irq;
    info->dev = device;
    info->handler = handler;

    yield_to(create_thread(irq_thread, (void *)((uintptr_t)malloc(CDI_IRQ_STACK_SIZE) + CDI_IRQ_STACK_SIZE), info));

    // FIXME: Das yield_to() reicht zwar hoffentlich aus, um den Handler
    // wirklich registriert zu haben, aber nicht unbedingt. Unbedingt wär
    // besser.
}


int cdi_reset_wait_irq(uint8_t irq)
{
    irqs_occured &= ~(1 << irq);

    return 0;
}


int cdi_wait_irq(uint8_t irq, uint32_t timeout)
{
    timeout += 10;

    // lol genauigkeit ist was für nuhbs
    while ((timeout >= 10) && !(irqs_occured & (1 << irq)))
    {
        msleep(10);
        timeout -= 10;
    }

    return (irqs_occured & (1 << irq)) ? 0 : -1;
}


void cdi_sleep_ms(uint32_t ms)
{
    msleep(ms);
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
