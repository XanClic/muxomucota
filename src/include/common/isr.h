#ifndef _ISR_H
#define _ISR_H


struct cpu_state;


void init_interrupts(void);

struct cpu_state *common_irq_handler(int irq, struct cpu_state *state);

void register_isr(int irq, struct cpu_state *(*isr)(int irq, struct cpu_state *state));

#endif
