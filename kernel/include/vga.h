#ifndef VGA_DRIVER_H
#define VGA_DRIVER_H
#include <stdint.h>

#define VGA_W 80
#define VGA_H 25

void vga_clear(void);
void vga_puts(const char* str);
void vga_putc(char c);
void vga_backspace(void);
void vga_set_color(uint8_t color);

void vga_scroll_up(void);
void vga_scroll_down(void);

#endif 