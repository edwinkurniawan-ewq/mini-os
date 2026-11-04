#include "vga.h"

#define VGA_W 80
#define VGA_H 25

static volatile uint16_t *const VGA = (volatile uint16_t *)0xB8000;
static int row = 0, col = 0;
static uint8_t color = 0x0F;

static void scroll(void) {
    for (int i = 0; i < VGA_W * (VGA_H - 1); i++)
        VGA[i] = VGA[i + VGA_W];
    for (int i = VGA_W * (VGA_H - 1); i < VGA_W * VGA_H; i++)
        VGA[i] = ((uint16_t)color << 8) | ' ';
    row = VGA_H - 1;
}

void vga_set_color(uint8_t c) { color = c; }

void vga_clear(void) {
    for (int i = 0; i < VGA_W * VGA_H; i++)
        VGA[i] = ((uint16_t)color << 8) | ' ';
    row = col = 0;
}

void vga_puts(const char *s) {
    for (; *s; s++) {
        if (*s == '\n') {
            col = 0;
            row++;
        } else {
            VGA[row * VGA_W + col] = ((uint16_t)color << 8) | (uint8_t)*s;
            if (++col == VGA_W) { col = 0; row++; }
        }
        if (row >= VGA_H) scroll();
    }
}