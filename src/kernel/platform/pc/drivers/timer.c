#include <ipc.h>
#include <isr.h>
#include <kassert.h>
#include <port-io.h>
#include <process.h>
#include <system-timer.h>


static volatile int tick_count = 0;


static void timer_isr(void);


void init_system_timer(void)
{
    int multiplier = 1193182 / SYSTEM_TIMER_FREQUENCY;

    register_kernel_isr(0, timer_isr);

    out8(0x43, 0x34);
    out8(0x40, multiplier & 0xFF);
    out8(0x40, multiplier >> 8);
}


static void timer_isr(void)
{
    tick_count += 1000 / SYSTEM_TIMER_FREQUENCY;

#ifndef COOPERATIVE
    yield();
#endif
}


void sleep(int ms)
{
    int end = tick_count + ms;

    while (tick_count < end)
        yield();
}
