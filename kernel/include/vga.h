#ifndef VGA_H_HEADER
#define VGA_H_HEADER

#include <stddef.h>
#include <stdint.h>

#define VGA_W 80
#define VGA_H 25

void vga_clear(void);
void vga_puts(const char* str);
void vga_putc(char c);
void vga_backspace(void);
void vga_set_color(uint8_t color);

#endif