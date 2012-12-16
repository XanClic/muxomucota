#include <isr.h>
#include <port-io.h>
#include <process.h>
#include <system-timer.h>


static struct cpu_state *timer_isr(int irq, struct cpu_state *state);


void init_system_timer(void)
{
    int multiplier = 1193182 / SYSTEM_TIMER_FREQUENCY;

    register_isr(0, timer_isr);

    out8(0x43, 0x34);
    out8(0x40, multiplier & 0xFF);
    out8(0x40, multiplier >> 8);
}


static struct cpu_state *timer_isr(int irq, struct cpu_state *state)
{
    (void)irq;

    return dispatch(state);
}
