#ifndef KERNEL_H
#define KERNEL_H

#include <stddef.h>
#include <stdint.h>

void vga_clear(void);
void vga_puts(const char* str);
void vga_set_color(uint8_t color);

#endif