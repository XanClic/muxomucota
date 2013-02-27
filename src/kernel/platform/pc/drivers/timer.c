#include <cpu-state.h>
#include <ipc.h>
#include <isr.h>
#include <kassert.h>
#include <port-io.h>
#include <process.h>
#include <system-timer.h>


volatile int elapsed_ms = 0;


static void timer_isr(struct cpu_state *state);


void init_system_timer(void)
{
    int multiplier = 1193182 / SYSTEM_TIMER_FREQUENCY;

    register_kernel_isr(0, timer_isr);

    out8(0x43, 0x34);
    out8(0x40, multiplier & 0xFF);
    out8(0x40, multiplier >> 8);
}


static void timer_isr(struct cpu_state *state)
{
    elapsed_ms += 1000 / SYSTEM_TIMER_FREQUENCY;

#ifdef COOPERATIVE
    (void)state;
#else
    // Den Kernelspace müssen wir nicht präemptieren. Hoffentlich.
    if (state->cs & 3)
        yield();
#endif
}


void sleep(int ms)
{
    int end = elapsed_ms + ms;

    while (elapsed_ms < end)
        yield();
}
