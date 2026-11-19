#include "vga.h"
#include "io.h"

#define MAX_HISTORY 200

static volatile uint16_t *const VGA = (volatile uint16_t *)0xB8000;
static uint16_t history[MAX_HISTORY][VGA_W];

static int row = 0;          // Baris aktif di dalam history (0 .. MAX_HISTORY-1)
static int col = 0;          // Kolom aktif (0 .. 79)
static int view_offset = 0;  // Baris teratas history yang sedang ditampilkan di layar
static uint8_t color = 0x0F;

static uint16_t vga_entry(char c, uint8_t col_attr) {
    return ((uint16_t)col_attr << 8) | (uint8_t)c;
}

static void update_cursor(void) {
    // Kursor hanya tampil kalau posisi ketik aktif masuk di layar yang sedang dilihat
    if (row >= view_offset && row < view_offset + VGA_H) {
        int screen_row = row - view_offset;
        uint16_t pos = screen_row * VGA_W + col;
        outb(0x3D4, 0x0F); outb(0x3D5, pos & 0xFF);
        outb(0x3D4, 0x0E); outb(0x3D5, (pos >> 8) & 0xFF);
    } else {
        // Sembunyikan kursor ke luar layar kalau user sedang scroll ke atas
        uint16_t pos = VGA_W * VGA_H;
        outb(0x3D4, 0x0F); outb(0x3D5, pos & 0xFF);
        outb(0x3D4, 0x0E); outb(0x3D5, (pos >> 8) & 0xFF);
    }
}

static void vga_refresh(void) {
    int max_offset = (row >= VGA_H) ? (row - VGA_H + 1) : 0;
    int is_scrolled = (view_offset < max_offset);

    for (int r = 0; r < VGA_H; r++) {
        int hist_idx = view_offset + r;
        for (int c = 0; c < VGA_W; c++) {
            if (hist_idx < MAX_HISTORY) {
                VGA[r * VGA_W + c] = history[hist_idx][c];
            } else {
                VGA[r * VGA_W + c] = vga_entry(' ', color);
            }
        }
    }

    if (is_scrolled) {
        /* Indikator riwayat gulir sesuai referensi */
        const char *badge = " RIWAYAT - gulir ke bawah / End ";
        int b_len = 0;
        while (badge[b_len]) b_len++;
        int start_col = VGA_W - b_len - 1;
        uint8_t badge_color = 0x4F; /* Latar Merah, Teks Putih */
        for (int i = 0; i < b_len; i++) {
            VGA[0 * VGA_W + (start_col + i)] = vga_entry(badge[i], badge_color);
        }
    }

    update_cursor();
}

static void newline(void) {
    col = 0;
    row++;

    // Kalau riwayat 200 baris penuh, geser ke atas 1 baris
    if (row >= MAX_HISTORY) {
        for (int r = 1; r < MAX_HISTORY; r++) {
            for (int c = 0; c < VGA_W; c++) {
                history[r - 1][c] = history[r][c];
            }
        }
        uint16_t blank = vga_entry(' ', color);
        for (int c = 0; c < VGA_W; c++) {
            history[MAX_HISTORY - 1][c] = blank;
        }
        row = MAX_HISTORY - 1;
    }

    // Ikuti baris paling bawah saat ada baris baru
    if (row >= VGA_H) {
        view_offset = row - VGA_H + 1;
    } else {
        view_offset = 0;
    }
    vga_refresh();
}

static void put_char(char c) {
    // Kalau user sedang lihat atas lalu mengetik, otomatis balik ke paling bawah
    int bottom_offset = (row >= VGA_H) ? (row - VGA_H + 1) : 0;
    if (view_offset != bottom_offset) {
        view_offset = bottom_offset;
        vga_refresh();
    }

    if (c == '\n') {
        newline();
        return;
    }

    history[row][col] = vga_entry(c, color);
    if (row >= view_offset && row < view_offset + VGA_H) {
        VGA[(row - view_offset) * VGA_W + col] = history[row][col];
    }

    if (++col == VGA_W) {
        newline();
    }
}

void vga_set_color(uint8_t c) { color = c; }

void vga_clear(void) {
    uint16_t blank = vga_entry(' ', color);
    for (int r = 0; r < MAX_HISTORY; r++) {
        for (int c = 0; c < VGA_W; c++) {
            history[r][c] = blank;
        }
    }
    row = col = view_offset = 0;
    vga_refresh();
}

void vga_putc(char c) {
    put_char(c);
    update_cursor();
}

void vga_put_int(int value) {
    char buf[12];
    int i = 0;

    if (value < 0) {
        vga_putc('-');
        value = -value;
    }
    if (value == 0) {
        vga_putc('0');
        return;
    }
    while (value > 0) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (i > 0) {
        vga_putc(buf[--i]);
    }
}

void vga_puts(const char *s) {
    for (; *s; s++) put_char(*s);
    update_cursor();
}

void vga_backspace(void) {
    if (col > 0) {
        col--;
    } else if (row > 0) {
        row--;
        col = VGA_W - 1;
    } else {
        return;
    }
    history[row][col] = vga_entry(' ', color);
    if (row >= view_offset && row < view_offset + VGA_H) {
        VGA[(row - view_offset) * VGA_W + col] = history[row][col];
    }
    update_cursor();
}

void vga_scroll_up(void) {
    if (view_offset > 0) {
        view_offset -= 5;
        if (view_offset < 0) view_offset = 0;
        vga_refresh();
    }
}

void vga_scroll_down(void) {
    int max_offset = (row >= VGA_H) ? (row - VGA_H + 1) : 0;
    if (view_offset < max_offset) {
        view_offset += 5;
        if (view_offset > max_offset) view_offset = max_offset;
        vga_refresh();
    }
}

void vga_scroll_home(void) {
    view_offset = 0;
    vga_refresh();
}

void vga_scroll_end(void) {
    int max_offset = (row >= VGA_H) ? (row - VGA_H + 1) : 0;
    view_offset = max_offset;
    vga_refresh();
}