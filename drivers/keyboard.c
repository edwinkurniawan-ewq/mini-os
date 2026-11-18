#include <stdint.h>
#include "io.h"
#include "vga.h"

static const char normal[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b', '\t',
    'q','w','e','r','t','y','u','i','o','p','[',']', '\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
    'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' '
};
static const char shifted[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b', '\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}', '\n', 0,
    'A','S','D','F','G','H','J','K','L',':','"','~', 0, '|',
    'Z','X','C','V','B','N','M','<','>','?', 0, '*', 0, ' '
};

static int shift = 0;

/* Helper komunikasi PS/2 Controller */
static void ps2_wait_write(void) {
    int timeout = 100000;
    while (timeout-- && (inb(0x64) & 2));
}

static void ps2_wait_read(void) {
    int timeout = 100000;
    while (timeout-- && !(inb(0x64) & 1));
}

static void mouse_write(uint8_t data) {
    ps2_wait_write();
    outb(0x64, 0xD4); /* Arahkan perintah berikutnya ke perangkat mouse */
    ps2_wait_write();
    outb(0x60, data);
    ps2_wait_read();
    inb(0x60);        /* Baca byte ACK (0xFA) */
}

static void mouse_init(void) {
    /* Aktifkan port auxiliary (mouse) pada PS/2 controller */
    ps2_wait_write();
    outb(0x64, 0xA8);

    /* Reset & set default */
    mouse_write(0xF6);

    /* Urutan wajib (Magic Sequence) untuk mengaktifkan Scroll Wheel (IntelliMouse ID 3) */
    mouse_write(0xF3); mouse_write(200);
    mouse_write(0xF3); mouse_write(100);
    mouse_write(0xF3); mouse_write(80);

    /* Aktifkan pengiriman paket data dari mouse */
    mouse_write(0xF4);
}

void keyboard_init(void) {
    while (inb(0x64) & 1) inb(0x60); /* Kosongkan buffer awal */
    mouse_init();                    /* Aktifkan mouse + scroll wheel */
    while (inb(0x64) & 1) inb(0x60); /* Bersihkan sisa ACK */
}

/* Menunggu input dari keyboard (termasuk PageUp/PageDown) atau scroll mouse */
char keyboard_getchar(void) {
    static uint8_t mouse_cycle = 0;
    static uint8_t mouse_packet[4];

    for (;;) {
        uint8_t status = inb(0x64);
        if (!(status & 1)) continue; /* Tunggu sampai ada data di buffer */

        uint8_t data = inb(0x60);

        /* Bit 5 (0x20) bernilai 1 artinya data berasal dari Mouse/Trackpad */
        if (status & 0x20) {
            /* Validasi byte pertama paket mouse (bit 3 selalu 1) */
            if (mouse_cycle == 0 && !(data & 0x08)) continue;

            mouse_packet[mouse_cycle++] = data;

            /* Paket IntelliMouse terdiri dari 4 byte */
            if (mouse_cycle == 4) {
                mouse_cycle = 0;
                int8_t scroll = (int8_t)mouse_packet[3]; /* Byte ke-4 adalah arah scroll */

                if (scroll < 0) {
                    vga_scroll_up();   /* Scroll ke atas */
                } else if (scroll > 0) {
                    vga_scroll_down(); /* Scroll ke bawah */
                }
            }
            continue;
        }

        /* Jika bukan mouse, berarti data dari Keyboard */
        mouse_cycle = 0;

        /* Deteksi Extended Key (0xE0) untuk Page Up / Page Down */
        if (data == 0xE0) {
            while (!(inb(0x64) & 1)) { }
            uint8_t ext = inb(0x60);
            if (ext == 0x49) {        /* Page Up */
                vga_scroll_up();
            } else if (ext == 0x51) { /* Page Down */
                vga_scroll_down();
            }
            continue;
        }

        if (data == 0x2A || data == 0x36) { shift = 1; continue; }
        if (data == 0xAA || data == 0xB6) { shift = 0; continue; }
        if (data & 0x80) continue;    /* Key release */

        char c = shift ? shifted[data] : normal[data];
        if (c) return c;
    }
}