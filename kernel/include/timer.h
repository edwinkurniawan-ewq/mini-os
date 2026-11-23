#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_init(void);
void timer_tick(void);
/* Majukan tick dari counter PIT tanpa bergantung IRQ0 (IDT masih stub). */
void timer_poll_ticks(void);
uint32_t timer_get_ticks(void);
void timer_get_time_str(char *buf, int maxlen);

#endif /* TIMER_H */
