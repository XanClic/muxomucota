#include <ipc.h>
#include <isr.h>
#include <port-io.h>
#include <process.h>
#include <system-timer.h>


#ifndef COOPERATIVE
static struct cpu_state *timer_isr(int irq, struct cpu_state *state);
#endif


void init_system_timer(void)
{
    int multiplier = 1193182 / SYSTEM_TIMER_FREQUENCY;

#ifndef COOPERATIVE
    register_isr(0, timer_isr);
#endif

    out8(0x43, 0x34);
    out8(0x40, multiplier & 0xFF);
    out8(0x40, multiplier >> 8);
}


#ifndef COOPERATIVE
static struct cpu_state *timer_isr(int irq, struct cpu_state *state)
{
    (void)irq;

    // return dispatch(state); funktioniert interessanterweise nicht. Das schon.
    // Also gibts nichts zu fixen.

    yield();

    return state;
}
#endif
