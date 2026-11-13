#include "vga.h"
#include "io.h"

#define VGA_W 80
#define VGA_H 25

static volatile uint16_t *const VGA = (volatile uint16_t *)0xB8000;
static int row = 0, col = 0;
static uint8_t color = 0x0F;

static void update_cursor(void) {
    uint16_t pos = row * VGA_W + col;
    outb(0x3D4, 0x0F); outb(0x3D5, pos & 0xFF);
    outb(0x3D4, 0x0E); outb(0x3D5, (pos >> 8) & 0xFF);
}

static void scroll(void) {
    for (int i = 0; i < VGA_W * (VGA_H - 1); i++)
        VGA[i] = VGA[i + VGA_W];
    for (int i = VGA_W * (VGA_H - 1); i < VGA_W * VGA_H; i++)
        VGA[i] = ((uint16_t)color << 8) | ' ';
    row = VGA_H - 1;
}

static void put_char(char c) {
    if (c == '\n') {
        col = 0; row++;
    } else {
        VGA[row * VGA_W + col] = ((uint16_t)color << 8) | (uint8_t)c;
        if (++col == VGA_W) { col = 0; row++; }
    }
    if (row >= VGA_H) scroll();
}

void vga_set_color(uint8_t c) { color = c; }

void vga_clear(void) {
    for (int i = 0; i < VGA_W * VGA_H; i++)
        VGA[i] = ((uint16_t)color << 8) | ' ';
    row = col = 0;
    update_cursor();
}

void vga_putc(char c) { put_char(c); update_cursor(); }

void vga_puts(const char *s) {
    for (; *s; s++) put_char(*s);
    update_cursor();
}

void vga_backspace(void) {
    if (col > 0) col--;
    else if (row > 0) { row--; col = VGA_W - 1; }
    else return;
    VGA[row * VGA_W + col] = ((uint16_t)color << 8) | ' ';
    update_cursor();
}