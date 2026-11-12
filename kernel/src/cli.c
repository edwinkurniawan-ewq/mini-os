#include "kernel.h"
#include "cli.h"
#include "kernel.h"
#include "cli.h"
#include "thread.h"

void vga_putc(char c);
void vga_backspace(void);
char keyboard_getchar(void);

void cli_init(void) {
    vga_set_color(0x09);
    vga_clear();

     vga_puts("       /\\         ============================================================\n");
    vga_puts("      /  \\        __        __  ___  __        __ ___  _____      ___   ____  \n");
    vga_puts("     / /\\ \\       \\ \\      / / / _ \\ \\ \\      / /|_ _||_   _|    / _ \\ / ___| \n");
    vga_puts("    / /  \\ \\       \\ \\ /\\ / / | | | | \\ \\ /\\ / /  | |   | |     | | | |\\___ \\ \n");
    vga_puts("   / /____\\ \\       \\ V  V /  | |_| |  \\ V  V /   | |   | |     | |_| | ___) |\n");
    vga_puts("  /_/      \\_\\       \\_/\\_/    \\___/    \\_/\\_/   |___|  |_|      \\___/ |____/ \n");
    vga_puts("                  ============================================================\n");
    vga_puts("                     Sistem Operasi - Informatika ITK\n\n");

    vga_set_color(0x0F);
    vga_puts("-------------------------------------------------------------------\n");
    vga_puts(" TIM ANGGOTA KELOMPOK:\n");
    vga_puts("  1. Edwin Kurniawan Eka Julianda (11251082)\n");
    vga_puts("  2. Muhammad Rasya Semardatu\n");
    vga_puts("  3. Andika Jusuf Jeremy Mustari\n");
    vga_puts("-------------------------------------------------------------------\n\n");

    vga_set_color(0x09);
    vga_puts("Ketik 'help' untuk melihat daftar perintah.\n\n");
}

#define CMD_MAX 64

static int streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

// FUNGSI INI YANG DIUPDATE DARI LANGKAH 3
static void run_command(const char *cmd) {
    if (cmd[0] == 0) return;

    if (streq(cmd, "help")) {
        vga_puts("Perintah yang tersedia:\n");
        vga_puts("  help     - tampilkan daftar perintah\n");
        vga_puts("  version  - tampilkan versi WOWIT OS\n");
        vga_puts("  thread   - Uji Coba Pengelolaan Thread (CPMK 2)\n"); 
        vga_puts("  clear    - bersihkan layar\n");
    } else if (streq(cmd, "version")) {
        vga_puts("WOWIT OS v0.1 (x86 32-bit, Multiboot)\n");
        
    // PASTIKAN BLOK INI ADA DI KODEMU:
    } else if (streq(cmd, "thread")) { 
        run_thread_demo();
        
    } else if (streq(cmd, "clear")) {
        vga_clear();
    } else {
        vga_puts("Perintah tidak dikenal: ");
        vga_puts(cmd);
        vga_puts("\nKetik 'help' untuk daftar perintah.\n");
    }
}

void cli_run(void) {
    char buf[CMD_MAX];
    int len;

    for (;;) {
        vga_set_color(0x09);
        vga_puts("wowit-os> ");
        vga_set_color(0x0F);

        len = 0;
        for (;;) {
            char c = keyboard_getchar();
            if (c == '\n') { vga_putc('\n'); break; }
            if (c == '\b') {
                if (len > 0) { len--; vga_backspace(); }
            } else if (c >= 32 && c < 127 && len < CMD_MAX - 1) {
                buf[len++] = c;
                vga_putc(c);
            }
        }
        buf[len] = 0;
        run_command(buf);
    }
}