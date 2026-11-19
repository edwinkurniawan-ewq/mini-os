#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_init(void);
void timer_tick(void);
uint32_t timer_get_ticks(void);
void timer_get_time_str(char *buf, int maxlen);

#endif /* TIMER_H */
