#ifndef _ISR_HPP
#define _ISR_HPP

#include <cpu-state.hpp>
#include <process.hpp>


void init_interrupts(void);

int common_irq_handler(int irq, cpu_state *state);

void irq_settled(int irq);

void register_kernel_isr(int irq, void (*isr)(cpu_state *state));
void unregister_isr_process(process *proc);

template<typename T> void register_user_isr(int irq, process *proc, void (__attribute__((regparm(1))) *process_handler)(T *info), T *info);

template<> void register_user_isr(int irq, process *proc, void (__attribute__((regparm(1))) *process_handler)(void *info), void *info);
template<typename T> void register_user_isr(int irq, process *proc, void (__attribute__((regparm(1))) *process_handler)(T *info), T *info)
{
    register_user_isr(irq, proc, reinterpret_cast<void (__attribute__((regparm(1))) *)(void *)>(process_handler), info);
}

#endif
