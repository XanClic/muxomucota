#ifndef _ISR_H
#define _ISR_H

#include <cpu-state.h>
#include <process.h>


struct cpu_state;


void init_interrupts(void);

int common_irq_handler(int irq, struct cpu_state *state);

void irq_settled(int irq);

void register_kernel_isr(int irq, void (*isr)(struct cpu_state *state));
void register_user_isr(int irq, process_t *process, void (*process_handler)(void *info), void *info);
void unregister_isr_process(process_t *process);

#endif
