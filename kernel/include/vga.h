#ifndef VGA_DRIVER_H
#define VGA_DRIVER_H
#include <stdint.h>

#define VGA_W 80
#define VGA_H 25

void vga_clear(void);
void vga_puts(const char* str);
void vga_putc(char c);
void vga_put_int(int value);
void vga_backspace(void);
void vga_set_color(uint8_t color);

void vga_scroll_up(void);
void vga_scroll_down(void);
void vga_scroll_home(void);
void vga_scroll_end(void);

/* --- Widget kecil pojok kanan atas (Thread & Memory Monitor) ------------- */
/* Hanya area ini yang ditimpa; area kiri/bawah tetap menampilkan terminal.
 * Isi di balik widget disimpan dipulihkan saat widget ditutup. */
#define VGA_WIDGET_ROWS 13
#define VGA_WIDGET_COLS 48
#define VGA_WIDGET_ROW  1
#define VGA_WIDGET_COL  (VGA_W - VGA_WIDGET_COLS)

void vga_widget_enter(void);   /* simpan background + kunci scroll */
void vga_widget_leave(void);   /* pulihkan background */
void vga_widget_fill(int row, int col, int rows, int cols, uint8_t attr);
void vga_widget_write(int row, int col, const char *text, uint8_t attr);

#endif 