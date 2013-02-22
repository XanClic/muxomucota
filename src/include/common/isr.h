#ifndef _ISR_H
#define _ISR_H

#include <process.h>


struct cpu_state;


void init_interrupts(void);

void common_irq_handler(int irq);

void register_kernel_isr(int irq, void (*isr)(void));
void register_user_isr(int irq, process_t *process, void (*process_handler)(void *info), void *info);
void unregister_isr_process(process_t *process);

#endif
