#include "kernel.h"
#include "task.h"
#include "timer.h"

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

static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static uint8_t bcd_to_bin(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

void timer_get_time_str(char *buf, int maxlen) {
    if (maxlen < 9) return;

    /* Tunggu jika CMOS sedang melakukan update */
    int timeout = 10000;
    outb(0x70, 0x0A);
    while ((inb(0x71) & 0x80) && --timeout) { }

    uint8_t sec = cmos_read(0x00);
    uint8_t min = cmos_read(0x02);
    uint8_t hour = cmos_read(0x04);
    uint8_t reg_b = cmos_read(0x0B);

    if (!(reg_b & 0x04)) {
        sec = bcd_to_bin(sec);
        min = bcd_to_bin(min);
        hour = bcd_to_bin(hour);
    }

    /* Sesuaikan ke WIB (UTC+7) jika diperlukan */
    hour = (hour + 7) % 24;

    buf[0] = '0' + (hour / 10);
    buf[1] = '0' + (hour % 10);
    buf[2] = ':';
    buf[3] = '0' + (min / 10);
    buf[4] = '0' + (min % 10);
    buf[5] = ':';
    buf[6] = '0' + (sec / 10);
    buf[7] = '0' + (sec % 10);
    buf[8] = '\0';
}

/* Called from IRQ0 handler — wired by Edwin in Week 2 */
void timer_tick(void) {
    tick_count++;
    schedule();               /* preemptive switch — fires when IDT/IRQ0 wired */
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
