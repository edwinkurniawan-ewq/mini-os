#include "kernel.h"
#include "cli.h"
#include "thread.h"
#include "finance.h"
#include "protect.h"
#include "task.h"
#include "logbook.h"
#include "io.h"
#include "tests.h"

void vga_putc(char c);
void vga_backspace(void);
char keyboard_getchar(void);

#define CMD_MAX 64

static int streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static int starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str++ != *prefix++) return 0;
    }
    return (*str == ' ' || *str == '\0');
}

static void print_menu_user(void) {
    vga_set_color(0x09); /* Biru terang sama seperti logo */
    vga_puts("=== MENU UTAMA USER ===\n");
    vga_set_color(0x0F);
    vga_puts("  help        - tampilkan bantuan ini\n");
    vga_puts("  kas         - aplikasi pencatat keuangan\n");
    vga_puts("  info        - informasi sistem\n");
    vga_puts("  mode        - tampilkan mode; 'mode kernel' pindah ke mode KERNEL\n");
    vga_puts("  clear       - bersihkan layar\n");
    vga_puts("  echo <teks> - cetak teks\n");
    vga_puts("  reboot      - restart sistem\n");
    vga_puts("  thread      - uji coba siklus thread (CPMK 2)\n");
    vga_puts("  deadlock    - uji coba mutex & deadlock (CPMK 5)\n");
}

static void print_menu_kernel(void) {
    vga_set_color(0x0C);
    vga_puts("=== MENU UTAMA KERNEL ===\n");
    vga_set_color(0x0F);
    vga_puts("  help     - tampilkan bantuan ini\n");
    vga_puts("  threads  - tampilkan daftar thread & status\n");
    vga_puts("  log      - baca buku log aktivitas kas\n");
    vga_puts("  laporan  - laporan kas oleh 2 thread\n");
    vga_puts("  izin     - akses sistem kas on and off\n");
    vga_puts("  exit     - kembali ke mode USER\n");
    vga_puts("  (perintah Menu USER juga tetap bisa dipakai di mode ini)\n");
}

static void system_reboot(void) {
    vga_puts("Merestart sistem...\n");
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);
    __asm__ volatile ("lidt (%esp)");
    __asm__ volatile ("int3");
}

static void do_info(void) {
    vga_set_color(0x0E);
    vga_puts("\n=== INFORMASI WOWIT OS ===\n");
    vga_set_color(0x0F);
    vga_puts(" Nama Sistem  : WOWIT OS v1.0 (Bare-Metal x86 32-bit)\n");
    vga_puts(" Mode Saat Ini: ");
    if (protect_get_mode() == MODE_KERNEL) {
        vga_set_color(0x0C); vga_puts("KERNEL (#)\n"); vga_set_color(0x0F);
    } else {
        vga_set_color(0x0A); vga_puts("USER (:)\n"); vga_set_color(0x0F);
    }
    vga_puts(" Progres Tim  : Bootloader, CLI Shell, Thread (CPMK 2), Deadlock (CPMK 5), App Kas\n");
    vga_puts(" Tim Anggota  :\n");
    vga_puts("   1. Edwin Kurniawan Eka Julianda (11251082) - PIC 1 (Core, Driver, CLI)\n");
    vga_puts("   2. Muhammad Rasya Semardatu                 - PIC 2 (Thread & Concurrency)\n");
    vga_puts("   3. Andika Jusuf Jeremy Mustari              - PIC 3 (Memori & VFS)\n\n");
}

static void handle_izin_command(const char *args) {
    while (*args == ' ') args++;
    if (*args == '\0') {
        protect_print_perms();
        return;
    }

    char fitur[16];
    int fi = 0;
    while (*args && *args != ' ' && fi < 15) {
        fitur[fi++] = *args++;
    }
    fitur[fi] = '\0';

    while (*args == ' ') args++;
    char val[8];
    int vi = 0;
    while (*args && *args != ' ' && vi < 7) {
        val[vi++] = *args++;
    }
    val[vi] = '\0';

    int enable = -1;
    if (streq(val, "on") || streq(val, "ON") || streq(val, "1")) {
        enable = 1;
    } else if (streq(val, "off") || streq(val, "OFF") || streq(val, "0")) {
        enable = 0;
    }

    if (enable == -1 || !protect_set_perm_by_name(fitur, enable)) {
        vga_set_color(0x0C);
        vga_puts("[!] Format salah. Contoh: izin hapus off / izin tambah on\n");
        vga_set_color(0x0F);
    }
    protect_print_perms();
}

static void run_command(const char *cmd) {
    if (cmd[0] == 0) return;

    os_mode_t current_mode = protect_get_mode();

    if (streq(cmd, "help")) {
        if (current_mode == MODE_KERNEL) {
            print_menu_kernel();
        } else {
            print_menu_user();
        }
    } else if (streq(cmd, "kas")) {
        finance_run();
    } else if (streq(cmd, "info")) {
        do_info();
    } else if (streq(cmd, "mode")) {
        vga_puts("Mode saat ini: ");
        if (current_mode == MODE_KERNEL) {
            vga_puts("KERNEL (#). Ketik 'exit' untuk kembali ke mode USER.\n");
        } else {
            vga_puts("USER (:). Ketik 'mode kernel' untuk pindah ke mode KERNEL.\n");
        }
    } else if (streq(cmd, "mode kernel")) {
        protect_set_mode(MODE_KERNEL);
        vga_set_color(0x0C);
        vga_puts("[MODE] USER -> KERNEL\n");
        vga_set_color(0x0F);
        print_menu_kernel();
    } else if (streq(cmd, "exit")) {
        if (current_mode == MODE_KERNEL) {
            protect_set_mode(MODE_USER);
            vga_set_color(0x09);
            vga_puts("[MODE] KERNEL -> USER\n");
            vga_set_color(0x0F);
            print_menu_user();
        } else {
            vga_puts("Sudah berada di mode USER.\n");
        }
    } else if (streq(cmd, "threads")) {
        if (current_mode != MODE_KERNEL) {
            vga_set_color(0x0C);
            vga_puts("Perintah tidak dikenal: ");
            vga_puts(cmd);
            vga_puts("\n(Perintah 'threads' hanya tersedia di mode KERNEL. Ketik 'mode kernel')\n");
            vga_set_color(0x0F);
        } else {
            task_print_threads();
        }
    } else if (streq(cmd, "log")) {
        if (current_mode != MODE_KERNEL) {
            vga_set_color(0x0C);
            vga_puts("Perintah tidak dikenal: ");
            vga_puts(cmd);
            vga_puts("\n(Perintah 'log' hanya tersedia di mode KERNEL. Ketik 'mode kernel')\n");
            vga_set_color(0x0F);
        } else {
            logbook_print();
        }
    } else if (streq(cmd, "laporan")) {
        if (current_mode != MODE_KERNEL) {
            vga_set_color(0x0C);
            vga_puts("Perintah tidak dikenal: ");
            vga_puts(cmd);
            vga_puts("\n(Perintah 'laporan' hanya tersedia di mode KERNEL. Ketik 'mode kernel')\n");
            vga_set_color(0x0F);
        } else {
            task_run_laporan();
        }
    } else if (starts_with(cmd, "izin")) {
        if (current_mode != MODE_KERNEL) {
            vga_set_color(0x0C);
            vga_puts("Perintah tidak dikenal: ");
            vga_puts(cmd);
            vga_puts("\n(Perintah 'izin' hanya tersedia di mode KERNEL. Ketik 'mode kernel')\n");
            vga_set_color(0x0F);
        } else {
            handle_izin_command(cmd + 4);
        }
    } else if (streq(cmd, "clear")) {
        vga_clear();
    } else if (starts_with(cmd, "echo")) {
        const char *text = cmd + 4;
        while (*text == ' ') text++;
        vga_puts(text);
        vga_putc('\n');
    } else if (streq(cmd, "reboot")) {
        system_reboot();
    } else if (streq(cmd, "thread")) {
        run_thread_demo();
    } else if (streq(cmd, "deadlock")) {
        run_deadlock_demo();
    } else if (streq(cmd, "memtest") || streq(cmd, "vfs") || streq(cmd, "vfstest") || streq(cmd, "protect") || streq(cmd, "race") || streq(cmd, "sched")) {
        test_malloc();
    } else {
        vga_set_color(0x0C);
        vga_puts("Perintah tidak dikenal: ");
        vga_puts(cmd);
        vga_puts("\n");
        vga_set_color(0x0F);
        vga_puts("Ketik 'help' untuk daftar perintah.\n");
    }
}

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

void cli_run(void) {
    char buf[CMD_MAX];
    int len;

    for (;;) {
        if (protect_get_mode() == MODE_KERNEL) {
            vga_set_color(0x0C); /* Merah untuk mode KERNEL */
            vga_puts("wowit-os#> ");
        } else {
            vga_set_color(0x09); /* Biru terang untuk mode USER (sama seperti logo) */
            vga_puts("wowit-os:> ");
        }
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
