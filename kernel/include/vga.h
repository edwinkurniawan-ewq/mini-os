#ifndef VGA_H
#define VGA_H
#include <stdint.h>

void vga_set_color(uint8_t c);
void vga_clear(void);
void vga_puts(const char *s);

#endif