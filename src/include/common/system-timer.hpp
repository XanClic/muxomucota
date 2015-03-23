#ifndef _SYSTEM_TIMER_HPP
#define _SYSTEM_TIMER_HPP

#define SYSTEM_TIMER_FREQUENCY 100


void init_system_timer(void);

void sleep(int ms);

#ifndef KERNEL
int elapsed_ms(void);
#endif

#endif
