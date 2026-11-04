#include "kernel.h"
#include "cli.h"

void cli_init(void) {
    // 0x09 = Biru Terang (Light Blue)
    vga_set_color(0x09);
    vga_clear();

vga_puts("__        __   ___  __        __  ___  _____     ___   ____  \n");
vga_puts("\\ \\      / /  / _ \\ \\ \\      / / |_ _||_   _|   / _ \\ / ___| \n");
vga_puts(" \\ \\ /\\ / /  | | | | \\ \\ /\\ / /   | |   | |   | | | |\\___ \\ \n");
vga_puts("  \\ V  V /   | |_| |  \\ V  V /    | |   | |   | |_| | ___) |\n");
vga_puts("   \\_/\\_/     \\___/    \\_/\\_/    |___|  |_|     \\___/ |____/ \n");
    
    vga_puts("Selamat datang di Wowit OS !\n");
    vga_puts("Ketik 'help' untuk melihat daftar perintah.\n\n");
}

void cli_run(void) {
    vga_puts("wowit-os> ");
}