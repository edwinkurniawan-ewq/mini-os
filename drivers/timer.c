#include "kernel.h"

/*
 * Programmable Interval Timer (8253/8254) — Channel 0
 * Base frequency: 1 193 182 Hz
 * Target tick rate: 100 Hz  ->  divisor = 11931
 * Mode 2 (rate generator): IRQ0 fires every divisor cycles
 *
 * NOTE: IRQ0 handler registered by Edwin's IDT (Week 2).
 *       timer_tick() called from that handler.
 */

#define PIT_BASE_FREQ   1193182UL
#define TICK_RATE_HZ    100
#define PIT_CMD         0x43   /* command/mode register */
#define PIT_CH0_DATA    0x40   /* channel 0 data port */
/* channel 0 | lobyte+hibyte access | mode 2 | binary */
#define PIT_MODE_RATE   0x34

static volatile uint32_t tick_count = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

/* Called from IRQ0 handler — wired by Edwin in Week 2 */
void timer_tick(void) {
    tick_count++;
    /* ponytail: call scheduler (schedule()) here in Week 2 after switch.asm */
}

uint32_t timer_get_ticks(void) {
    return tick_count;
}

void timer_init(void) {
    uint16_t divisor = (uint16_t)(PIT_BASE_FREQ / TICK_RATE_HZ);
    outb(PIT_CMD,      PIT_MODE_RATE);
    outb(PIT_CH0_DATA, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0_DATA, (uint8_t)(divisor >> 8));
    vga_puts("[TIMER] PIT configured @ 100 Hz\n");
}
