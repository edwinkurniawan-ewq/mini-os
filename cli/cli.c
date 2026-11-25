#include "kernel.h"
#include "cli.h"
#include "vga.h"
#include "thread.h"
#include "finance.h"
#include "protect.h"
#include "task.h"
#include "logbook.h"
#include "io.h"
#include "tests.h"
#include "rr.h"
#include "memory.h"
#include "vfs.h"
#include "timer.h"
#include "monitor.h"

char keyboard_getchar(void);

#define CMD_MAX      64
#define PASSWORD_MAX 32
#define MAX_ATTEMPTS 3
#define KERNEL_PASS  "wowit123"   /* Ubah di sini untuk mengganti password */

/* ---- helper tanpa libc --------------------------------------------------- */

static int streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static const char *skip_spaces(const char *p) {
    while (*p == ' ') p++;
    return p;
}

/* Ambil satu token dipisah spasi; kembalikan 0 bila tidak ada token. */
static int copy_token(const char **cursor, char *dst, int max) {
    const char *p = skip_spaces(*cursor);
    int n = 0;

    while (*p && *p != ' ' && n < max - 1) dst[n++] = *p++;
    dst[n] = '\0';
    *cursor = p;
    return n > 0;
}

static int starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str++ != *prefix++) return 0;
    }
    return (*str == ' ' || *str == '\0');
}

/* Ambil bilangan desimal berikutnya, aman dari overflow 32-bit. */
static int scan_uint(const char **cursor, uint32_t *out) {
    const char *p = *cursor;
    uint32_t value = 0;
    int found = 0;

    while (*p == ' ') p++;
    while (*p >= '0' && *p <= '9') {
        if (value > 10000000u) return 0;
        value = value * 10u + (uint32_t)(*p - '0');
        found = 1;
        p++;
    }
    if (!found) return 0;

    *out = value;
    *cursor = p;
    return 1;
}

static void read_line(char *buf, int maxlen) {
    int len = 0;

    for (;;) {
        char c = keyboard_getchar();
        if (c == '\n') {
            vga_putc('\n');
            break;
        }
        if (c == '\b') {
            if (len > 0) {
                len--;
                vga_backspace();
            }
            continue;
        }
        if (c >= 32 && c < 127 && len < maxlen - 1) {
            buf[len++] = c;
            vga_putc(c);
        }
    }
    buf[len] = '\0';
}

/* ---- menu --------------------------------------------------------------- */

static void print_menu_user(void) {
    vga_set_color(0x09);
    vga_puts("=== MENU UTAMA USER (wowit-os>) ===\n");
    vga_set_color(0x0F);
    vga_puts("  info   - identitas & spesifikasi dasar WOWIT OS\n");
    vga_puts("  kas    - aplikasi pencatat keuangan (CRUD)\n");
    vga_puts("  help   - tampilkan  perintah ini\n");
    vga_puts("  exit   - shutdown / halt sistem\n");
}

static void print_menu_kernel(void) {
    vga_set_color(0x0E);
    vga_puts("=== MENU UTAMA KERNEL (wowit-kernel#) ===\n");

    /* ---- perintah User Mode juga bisa langsung dipakai di sini ---- */
    vga_set_color(0x0B);
    vga_puts("=== PERINTAH USER MODE ===\n");
    vga_set_color(0x0F);
    vga_puts("  info   - identitas & spesifikasi dasar WOWIT OS\n");
    vga_puts("  kas    - aplikasi pencatat keuangan (CRUD)\n");
    vga_puts("  help   - tampilkan daftar perintah mode ini\n");
    vga_puts("  exit   - di Kernel: logout ke User | di User: shutdown\n");
    vga_puts("  logout - kembali ke User Mode (wowit-os>)\n");

    /* ---- perintah khusus kernel ---- */
    vga_set_color(0x0E);
    vga_puts("=== PERINTAH KERNEL MODE ===\n");
    vga_set_color(0x0F);
    vga_puts("  help         - daftar perintah kernel ini\n");
    vga_puts("  thread       - uji siklus thread (CPMK 2)\n");
    vga_puts("  deadlock     - uji mutex & deadlock + deteksi (CPMK 5)\n");
    vga_puts("  deadlock-sim - simulasi deadlock dua kasir\n");
    vga_puts("  sync         - uji mutex aman (tanpa lost update)\n");
    vga_puts("  race         - uji race condition tanpa mutex\n");
    vga_puts("  semaphore    - uji counting semaphore\n");
    vga_puts("  sched        - uji penjadwalan / lifecycle thread\n");
    vga_puts("  memtest      - uji alokasi memori kmalloc/kfree\n");
    vga_puts("  vfs          - uji VFS RAMDisk (create/read/update/delete)\n");
    vga_puts("  preempt-test - bukti preemption IRQ0: 2 thread CPU-bound tanpa yield\n");
    vga_puts("  ls           - daftar berkas RAMDisk\n");
    vga_puts("  cat <nama>   - baca isi berkas RAMDisk\n");
    vga_puts("  touch <nama> <isi> - buat/update berkas RAMDisk\n");
    vga_puts("  rm <nama>    - hapus berkas RAMDisk\n");
    vga_puts("  protect      - uji proteksi hak akses & audit log\n");
    vga_puts("    Uji manual: izin hapus off -> logout -> kas -> pilih 4 (hapus)\n");
    vga_puts("  threads      - daftar thread terdaftar\n");
    vga_puts("  log          - buku log aktivitas kas\n");
    vga_puts("  laporan      - laporan kas oleh 2 thread\n");
    vga_puts("  izin         - lihat/atur izin: izin hapus off\n");
    vga_puts("  quantum [n]  - lihat/ubah time quantum RR (1-1000 tick)\n");
    vga_puts("  test-rr AB   - RR fleksibel: AB, A:150 B:80, AB 120\n");
    vga_puts("  rr-sim       - simulasi cepat (4 task, burst 8 tick)\n");
    vga_puts("  clear        - bersihkan layar | echo <teks> | reboot\n");
}

/* ---- aksi sistem -------------------------------------------------------- */

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

static void system_halt(void) {
    vga_set_color(0x0E);
    vga_puts("\n=== SHUTDOWN WOWIT OS ===\n");
    vga_puts("Sistem dihentikan (halt). Aman untuk mematikan mesin.\n");
    vga_set_color(0x0F);
    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void do_info(void) {
    vga_set_color(0x0E);
    vga_puts("\n=== INFORMASI WOWIT OS ===\n");
    vga_set_color(0x0F);
    vga_puts(" Nama Sistem  : WOWIT OS v1.0 (Bare-Metal x86 32-bit)\n");
    vga_puts(" Prompt       : wowit-os> (USER) / wowit-kernel# (KERNEL)\n");
    vga_set_color(0x0C);
    vga_puts(" Mode Saat Ini: ");
    if (protect_get_mode() == MODE_KERNEL) {
        vga_puts("KERNEL (#)\n");
    } else {
        vga_set_color(0x0A);
        vga_puts("USER (:)\n");
    }
    vga_set_color(0x0F);
    vga_puts(" Arsitektur   : x86 32-bit, VGA teks 80x25, PIT 100 Hz\n");
    vga_puts(" Timer        : ");
    vga_put_int((int)timer_get_ticks());
    vga_puts(" tick sejak boot\n");
    vga_puts(" Heap         : ");
    vga_put_int((int)memory_get_total());
    vga_puts(" B total / ");
    vga_put_int((int)memory_get_free());
    vga_puts(" B bebas\n");
    vga_puts(" RAMDisk      : ");
    vga_put_int((int)vfs_get_total_bytes());
    vga_puts(" B total / ");
    vga_put_int((int)vfs_get_used_bytes());
    vga_puts(" B terpakai\n");
    vga_puts(" Progres Tim  : Bootloader, CLI, Thread, Deadlock, Memori, VFS, App Kas\n");
    vga_puts(" Tim Anggota  :\n");
    vga_puts("   1. Edwin Kurniawan Eka Julianda (11251082) - PIC 1 (Core, Driver, CLI)\n");
    vga_puts("   2. Muhammad Rasya Semardatu                 - PIC 2 (Thread & Concurrency)\n");
    vga_puts("   3. Andika Jusuf Jeremy Mustari              - PIC 3 (Memori & VFS)\n\n");
}

static void logout_to_user(void) {
    protect_set_mode(MODE_USER);
    vga_set_color(0x09);
    vga_puts("[MODE] KERNEL -> USER. Kembali ke wowit-os>\n");
    vga_set_color(0x0F);
}

/* ---- akses kernel tersembunyi ------------------------------------------- */

static void try_kernel_access(void) {
    char password[PASSWORD_MAX];

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        int len = 0;

        vga_set_color(0x0E);
        vga_puts("Password Kernel (percobaan ");
        vga_put_int(attempt);
        vga_puts("/3): ");
        vga_set_color(0x0F);

        for (;;) {
            char c = keyboard_getchar();
            if (c == '\n') {
                vga_putc('\n');
                password[len] = '\0';
                break;
            }
            if (c == '\b') {
                if (len > 0) {
                    len--;
                    vga_backspace();
                }
                continue;
            }
            if (c >= 32 && c < 127 && len < PASSWORD_MAX - 1) {
                password[len++] = c;
                vga_putc('*');   /* seluruh karakter disensor */
            }
        }

        if (streq(password, KERNEL_PASS)) {
            protect_set_mode(MODE_KERNEL);
            vga_set_color(0x0A);
            vga_puts("[AUTH] Password benar. Akses KERNEL diberikan.\n");
            vga_set_color(0x0F);
            print_menu_kernel();
            return;
        }

        vga_set_color(0x0C);
        vga_puts("[!] Password salah. ");
        if (attempt < MAX_ATTEMPTS) {
            vga_puts("Sisa percobaan: ");
            vga_put_int(MAX_ATTEMPTS - attempt);
            vga_puts("\n");
        } else {
            vga_puts("Batas 3 percobaan terlampaui. Akses diblokir.\n");
        }
        vga_set_color(0x0F);
    }

    vga_set_color(0x0C);
    vga_puts("[BLOKIR] Kembali ke User Mode (wowit-os>).\n");
    vga_set_color(0x0F);
}

/* ---- perintah kernel ---------------------------------------------------- */

static void handle_izin_command(const char *args) {
    char fitur[16];
    char val[8];
    int fi = 0, vi = 0;
    int enable = -1;

    while (*args == ' ') args++;
    if (*args == '\0') {
        protect_print_perms();
        return;
    }

    while (*args && *args != ' ' && fi < 15) fitur[fi++] = *args++;
    fitur[fi] = '\0';

    while (*args == ' ') args++;
    while (*args && *args != ' ' && vi < 7) val[vi++] = *args++;
    val[vi] = '\0';

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

/* ---- pendaftaran aktivitas ke tabel utama widget monitor ---------------- */

/* Jalankan perintah dan ukur durasinya dengan polling PIT. */
static void tracked_call(const char *name, void (*fn)(void)) {
    uint32_t before, elapsed;

    timer_poll_ticks();
    before = timer_get_ticks();
    monitor_activity_begin(name, 0u);   /* NEW */
    monitor_activity_ready();           /* READY */
    monitor_activity_run();             /* RUNNING */
    fn();
    timer_poll_ticks();
    elapsed = timer_get_ticks() - before;
    monitor_activity_end(elapsed);      /* TERMINATED */
}

/* Untuk pengujian yang menunggu resource: NEW -> READY -> RUNNING -> WAITING. */
static void tracked_wait_call(const char *name, void (*fn)(void)) {
    uint32_t before, elapsed;

    timer_poll_ticks();
    before = timer_get_ticks();
    monitor_activity_begin(name, 0u);
    monitor_activity_ready();
    monitor_activity_run();
    monitor_activity_wait();            /* WAITING */
    fn();
    timer_poll_ticks();
    elapsed = timer_get_ticks() - before;
    monitor_activity_end(elapsed);      /* TERMINATED */
}

static void note_quantum(uint32_t value) {
    char name[10];
    char digits[10];
    int i = 0, n = 0;

    name[i++] = 'q';
    name[i++] = '=';
    if (value == 0u) {
        digits[n++] = '0';
    } else {
        while (value > 0u && n < 10) {
            digits[n++] = (char)('0' + (value % 10u));
            value /= 10u;
        }
    }
    while (n > 0 && i < 9) name[i++] = digits[--n];
    name[i] = '\0';
    monitor_activity_add(name, rr_get_quantum());
}

static void note_rr_begin(const char *args) {
    char name[10];
    int i = 0;

    name[i++] = 'r';
    name[i++] = 'r';
    name[i++] = ':';
    while (*args && *args != ' ' && i < 9) name[i++] = *args++;
    name[i] = '\0';
    monitor_activity_begin(name, rr_get_quantum());  /* NEW */
    monitor_activity_ready();                        /* READY */
    monitor_activity_run();                          /* RUNNING */
}

static void handle_quantum(const char *args) {
    uint32_t old = rr_get_quantum();
    uint32_t next;

    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_puts("Time quantum saat ini: ");
        vga_put_int((int)old);
        vga_puts(" tick (");
        vga_put_int((int)(old * 10u));
        vga_puts(" ms)\n");
        vga_puts("Cara pakai: quantum <tick>, misalnya quantum 5 atau quantum 20\n");
        return;
    }

    if (!scan_uint(&args, &next) || !rr_set_quantum(next)) {
        vga_set_color(0x0C);
        vga_puts("[!] Quantum harus bilangan 1-1000 tick.\n");
        vga_set_color(0x0F);
        return;
    }

    vga_set_color(0x0A);
    vga_puts("[OK] Quantum diubah: ");
    vga_put_int((int)old);
    vga_puts(" -> ");
    vga_put_int((int)next);
    vga_puts(" tick\n");
    vga_set_color(0x0F);
    note_quantum(next);
}

static void handle_test_rr(const char *args) {
    rr_task_t tasks[RR_SIM_MAX_TASKS];
    int count;

    while (*args == ' ') args++;
    count = rr_parse_tasks(args, tasks, RR_SIM_MAX_TASKS);
    if (count == 0) {
        vga_set_color(0x0C);
        vga_puts("[!] Format tidak dikenal. Contoh:\n");
        vga_puts("    test-rr AB | test-rr ABCD | test-rr A:150 B:80\n");
        vga_puts("    test-rr AB 120 | test-rr 4 12\n");
        vga_set_color(0x0F);
        return;
    }

    note_rr_begin(args);
    rr_run(tasks, count);
    monitor_activity_end(rr_get_quantum());
}

/* ---- perintah VFS interaktif -------------------------------------------- */

static void handle_cat(const char *args) {
    char name[VFS_MAX_FILENAME];
    char buf[256];
    int n;

    if (!copy_token(&args, name, (int)sizeof(name))) {
        vga_set_color(0x0C);
        vga_puts("[!] Format: cat <nama_file>\n");
        vga_set_color(0x0F);
        return;
    }

    n = vfs_read(name, buf, (int)sizeof(buf) - 1);
    if (n < 0) {
        vga_set_color(0x0C);
        vga_puts("[!] Berkas tidak ditemukan: ");
        vga_puts(name);
        vga_puts("\n");
        vga_set_color(0x0F);
        return;
    }

    buf[n] = '\0';
    vga_puts(buf);
    vga_putc('\n');
}

static void handle_touch(const char *args) {
    char name[VFS_MAX_FILENAME];
    const char *content;
    int len;

    if (!copy_token(&args, name, (int)sizeof(name))) {
        vga_set_color(0x0C);
        vga_puts("[!] Format: touch <nama_file> [isi]\n");
        vga_set_color(0x0F);
        return;
    }

    content = skip_spaces(args);
    len = str_len(content);

    if (vfs_write(name, content, (size_t)len) != 0) {
        vga_set_color(0x0C);
        vga_puts("[!] Gagal menulis berkas: ");
        vga_puts(name);
        vga_puts("\n");
        vga_set_color(0x0F);
        return;
    }

    vga_set_color(0x0A);
    vga_puts("[OK] ");
    vga_puts(name);
    vga_puts (" (");
    vga_put_int(len);
    vga_puts(" B)\n");
    vga_set_color(0x0F);
}

static void handle_rm(const char *args) {
    char name[VFS_MAX_FILENAME];

    if (!copy_token(&args, name, (int)sizeof(name))) {
        vga_set_color(0x0C);
        vga_puts("[!] Format: rm <nama_file>\n");
        vga_set_color(0x0F);
        return;
    }

    if (vfs_delete(name) != 0) {
        vga_set_color(0x0C);
        vga_puts("[!] Berkas tidak ditemukan: ");
        vga_puts(name);
        vga_puts("\n");
        vga_set_color(0x0F);
        return;
    }

    vga_set_color(0x0A);
    vga_puts("[OK] dihapus: ");
    vga_puts(name);
    vga_puts("\n");
    vga_set_color(0x0F);
}

static void unknown_command(const char *cmd) {
    vga_set_color(0x0C);
    vga_puts("Perintah tidak dikenal: ");
    vga_puts(cmd);
    vga_puts("\n");
    vga_set_color(0x0F);
    vga_puts("Ketik 'help' untuk daftar perintah.\n");
}

static void run_command(const char *cmd) {
    os_mode_t mode;

    if (cmd[0] == '\0') return;
    mode = protect_get_mode();

    if (streq(cmd, "help")) {
        if (mode == MODE_KERNEL) print_menu_kernel();
        else print_menu_user();
    } else if (streq(cmd, "info")) {
        tracked_call("info", do_info);
    } else if (streq(cmd, "kas")) {
        tracked_call("kas", finance_run);
    } else if (streq(cmd, "exit")) {
        if (mode == MODE_KERNEL) logout_to_user();
        else system_halt();
    } else if (streq(cmd, "logout")) {
        if (mode == MODE_KERNEL) logout_to_user();
        else unknown_command(cmd);
    } else if (mode == MODE_USER && streq(cmd, "kernel mode")) {
        try_kernel_access();   /* perintah rahasia: tidak ada di help USER */
    } else if (mode == MODE_KERNEL) {
        if (streq(cmd, "thread")) {
            tracked_call("thread", run_thread_demo);
        } else if (streq(cmd, "deadlock")) {
            tracked_wait_call("deadlock", test_deadlock);
        } else if (streq(cmd, "deadlock-sim")) {
            tracked_wait_call("deadlock-sim", test_deadlock_sim);
        } else if (streq(cmd, "sync")) {
            tracked_wait_call("sync", test_sync);
        } else if (streq(cmd, "race")) {
            tracked_call("race", test_race);
        } else if (streq(cmd, "semaphore")) {
            tracked_wait_call("semaphore", test_semaphore);
        } else if (streq(cmd, "sched")) {
            tracked_call("sched", test_sched);
        } else if (streq(cmd, "memtest")) {
            tracked_call("memtest", test_malloc);
        } else if (streq(cmd, "vfs") || streq(cmd, "vfstest")) {
            tracked_call("vfs", test_vfs);
        } else if (streq(cmd, "preempt-test")) {
            tracked_call("preempt", task_preempt_demo);
        } else if (streq(cmd, "ls")) {
            vfs_list();
        } else if (starts_with(cmd, "cat")) {
            handle_cat(cmd + 3);
        } else if (starts_with(cmd, "touch")) {
            handle_touch(cmd + 5);
        } else if (starts_with(cmd, "rm")) {
            handle_rm(cmd + 2);
        } else if (streq(cmd, "protect")) {
            tracked_call("protect", test_protect);
        } else if (streq(cmd, "threads")) {
            tracked_call("threads", task_print_threads);
        } else if (streq(cmd, "log")) {
            tracked_call("log", logbook_print);
        } else if (streq(cmd, "laporan")) {
            tracked_call("laporan", task_run_laporan);
        } else if (starts_with(cmd, "izin")) {
            handle_izin_command(cmd + 4);
        } else if (starts_with(cmd, "quantum")) {
            handle_quantum(cmd + 7);
        } else if (starts_with(cmd, "test-rr")) {
            handle_test_rr(cmd + 7);
        } else if (streq(cmd, "rr-sim")) {
            monitor_activity_begin("rr-sim", rr_get_quantum());  /* NEW */
            monitor_activity_ready();                            /* READY */
            monitor_activity_run();                              /* RUNNING */
            rr_simulate(4u, 8u);
            monitor_activity_end(rr_get_quantum());
        } else if (streq(cmd, "clear")) {
            vga_clear();
        } else if (starts_with(cmd, "echo")) {
            const char *text = cmd + 4;
            while (*text == ' ') text++;
            vga_puts(text);
            vga_putc('\n');
        } else if (streq(cmd, "reboot")) {
            system_reboot();
        } else {
            unknown_command(cmd);
        }
    } else {
        unknown_command(cmd);
    }
}

/* ---- entry point CLI ---------------------------------------------------- */

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

    print_menu_user();
    vga_putc('\n');
}

void cli_run(void) {
    char buf[CMD_MAX];

    for (;;) {
        if (protect_get_mode() == MODE_KERNEL) {
            vga_set_color(0x0C);
            vga_puts("wowit-kernel# ");
        } else {
            vga_set_color(0x0B);
            vga_puts("wowit-os> ");
        }
        vga_set_color(0x0F);

        read_line(buf, CMD_MAX);
        run_command(buf);
    }
}
