#include "logbook.h"
#include "timer.h"
#include "kernel.h"

static log_entry_t logs[MAX_LOG_ENTRIES];
static int log_count = 0;
static int logger_active = 0;

static void l_strcpy(char *dst, const char *src, int max) {
    int i = 0;
    while (i < max - 1 && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void print_int_val(int num) {
    if (num == 0) {
        vga_puts("0");
        return;
    }
    char buf[12];
    int i = 0;
    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }
    char rev[12];
    for (int j = 0; j < i; j++) {
        rev[j] = buf[i - 1 - j];
    }
    rev[i] = '\0';
    vga_puts(rev);
}

void logbook_init(void) {
    log_count = 0;
    logger_active = 0;
}

void logbook_add(const char *aksi, int id, char tipe, const char *detail) {
    if (log_count >= MAX_LOG_ENTRIES) {
        /* Geser log lama jika penuh */
        for (int i = 1; i < MAX_LOG_ENTRIES; i++) {
            logs[i - 1] = logs[i];
            logs[i - 1].no = i;
        }
        log_count = MAX_LOG_ENTRIES - 1;
    }

    log_entry_t *entry = &logs[log_count];
    entry->no = log_count + 1;
    timer_get_time_str(entry->jam, sizeof(entry->jam));
    l_strcpy(entry->aksi, aksi, sizeof(entry->aksi));
    entry->id = id;
    entry->tipe = tipe;
    l_strcpy(entry->detail, detail, sizeof(entry->detail));

    log_count++;
}

void logbook_set_logger_active(int active) {
    logger_active = active;
}

int logbook_is_logger_active(void) {
    return logger_active;
}

int logbook_get_count(void) {
    return log_count;
}

void logbook_print(void) {
    vga_set_color(0x0C);
    vga_puts("=== BUKU LOG KAS ===\n");
    vga_puts("NO  JAM      AKSI   ID  DETAIL\n");
    vga_set_color(0x0F);

    for (int i = 0; i < log_count; i++) {
        log_entry_t *e = &logs[i];

        print_int_val(e->no);
        vga_puts(e->no < 10 ? "   " : "  ");

        vga_puts(e->jam);
        vga_puts(" ");

        vga_puts(e->aksi);
        int a_len = 0;
        while (e->aksi[a_len]) a_len++;
        for (int p = a_len; p < 7; p++) vga_puts(" ");

        if (e->id > 0) {
            print_int_val(e->id);
            vga_puts("   ");
        } else {
            vga_puts("-   ");
        }

        if (e->tipe == '+' || e->tipe == '-') {
            vga_putc(e->tipe);
            vga_puts(" ");
        }

        vga_puts(e->detail);
        vga_puts("\n");
    }

    vga_set_color(0x0C);
    vga_puts("Total: ");
    print_int_val(log_count);
    vga_puts(" Catatan Thread Logger: ");
    if (logger_active) {
        vga_puts("aktif (sesi kas berjalan)\n");
    } else {
        vga_puts("tidak aktif (tidak ada sesi kas)\n");
    }
    vga_set_color(0x0F);
}
