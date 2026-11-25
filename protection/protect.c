#include "protect.h"
#include "kernel.h"
#include "vga.h"
#include "irq.h"

#define PROTECT_AUDIT_MAX 8

static os_mode_t current_mode = MODE_USER;
static int permissions[PERM_COUNT] = {1, 1, 1, 1, 1};

/* Audit log penolakan akses (Minggu 3) */
static struct {
    perm_type_t perm;
    int         denied;
} audit_log[PROTECT_AUDIT_MAX];
static int audit_total = 0;
static int crit_depth  = 0;
static uint32_t crit_flags = 0;   /* EFLAGS saat section outermost dimasuki */

static const char *perm_name(perm_type_t perm) {
    switch (perm) {
        case PERM_TAMBAH: return "tambah";
        case PERM_LIHAT:  return "lihat";
        case PERM_UBAH:   return "ubah";
        case PERM_HAPUS:  return "hapus";
        case PERM_SIMPAN: return "simpan";
        default:          return "?";
    }
}

static void audit_add(perm_type_t perm) {
    audit_total++;
    for (int i = 0; i < PROTECT_AUDIT_MAX; i++) {
        if (audit_log[i].denied > 0 && audit_log[i].perm == perm) {
            audit_log[i].denied++;
            return;
        }
    }
    for (int i = 0; i < PROTECT_AUDIT_MAX; i++) {
        if (audit_log[i].denied == 0) {
            audit_log[i].perm   = perm;
            audit_log[i].denied = 1;
            return;
        }
    }
}

static int p_streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

void protect_init(void) {
    current_mode = MODE_USER;
    for (int i = 0; i < PERM_COUNT; i++) {
        permissions[i] = 1;
    }
    for (int i = 0; i < PROTECT_AUDIT_MAX; i++) {
        audit_log[i].perm   = PERM_LIHAT;
        audit_log[i].denied = 0;
    }
    audit_total = 0;
    crit_depth  = 0;
    crit_flags  = 0;
}

os_mode_t protect_get_mode(void) {
    return current_mode;
}

void protect_set_mode(os_mode_t mode) {
    current_mode = mode;
}

int protect_check_perm(perm_type_t perm) {
    if (current_mode == MODE_KERNEL) {
        return 1; /* Kernel selalu memiliki izin penuh */
    }
    if (perm >= 0 && perm < PERM_COUNT) {
        if (permissions[perm]) return 1;
        audit_add(perm);     /* penolakan dicatat di audit log */
        return 0;
    }
    return 0;
}

int protect_set_perm_by_name(const char *name, int enable) {
    if (p_streq(name, "tambah")) {
        permissions[PERM_TAMBAH] = enable;
    } else if (p_streq(name, "transaksi") || p_streq(name, "lihat") || p_streq(name, "barang")) {
        permissions[PERM_LIHAT] = enable;
    } else if (p_streq(name, "ubah")) {
        permissions[PERM_UBAH] = enable;
    } else if (p_streq(name, "hapus")) {
        permissions[PERM_HAPUS] = enable;
    } else if (p_streq(name, "simpan")) {
        permissions[PERM_SIMPAN] = enable;
    } else {
        return 0;
    }
    return 1;
}

void protect_print_perms(void) {
    vga_set_color(0x0C);
    vga_puts("Izin mode USER:\n");
    vga_puts("  tambah    : "); vga_puts(permissions[PERM_TAMBAH] ? "ON\n" : "OFF\n");
    vga_puts("  transaksi : "); vga_puts(permissions[PERM_LIHAT] ? "ON\n" : "OFF\n");
    vga_puts("  ubah      : "); vga_puts(permissions[PERM_UBAH] ? "ON\n" : "OFF\n");
    vga_puts("  hapus     : "); vga_puts(permissions[PERM_HAPUS] ? "ON\n" : "OFF\n");
    vga_puts("  simpan    : "); vga_puts(permissions[PERM_SIMPAN] ? "ON\n" : "OFF\n");
    vga_set_color(0x0F);
}

int protect_require(perm_type_t perm, const char *action) {
    if (protect_check_perm(perm)) return 1;

    vga_set_color(0x0C);
    vga_puts("[PROTECT] AKSES DITOLAK: ");
    vga_puts(action);
    vga_puts(" (izin ");
    vga_puts(perm_name(perm));
    vga_puts(" dimatikan)\n");
    vga_set_color(0x0F);
    return 0;
}

void protect_enter_critical(void) {
    if (crit_depth == 0) {
        crit_flags = irq_save_disable();
    }
    crit_depth++;
}

void protect_exit_critical(void) {
    if (crit_depth <= 0) return;

    crit_depth--;
    if (crit_depth == 0) {
        /* Pulihkan IF persis seperti sebelum section dimasuki. Jika
         * interupsi memang sudah mati, EFLAGS dikembalikan dengan IF=0. */
        irq_restore(crit_flags);
    }
}

void protect_print_audit(void) {
    vga_set_color(0x0C);
    vga_puts("Audit penolakan akses: ");
    vga_put_int(audit_total);
    vga_puts(" kali\n");
    vga_set_color(0x0F);

    for (int i = 0; i < PROTECT_AUDIT_MAX; i++) {
        if (audit_log[i].denied == 0) continue;
        vga_puts("  izin ");
        vga_puts(perm_name(audit_log[i].perm));
        vga_puts(" ditolak ");
        vga_put_int(audit_log[i].denied);
        vga_puts("x\n");
    }
}

int protect_denied_count(void) {
    return audit_total;
}
