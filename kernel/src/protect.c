#include "protect.h"
#include "kernel.h"

static os_mode_t current_mode = MODE_USER;
static int permissions[PERM_COUNT] = {1, 1, 1, 1, 1};

static int p_streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

void protect_init(void) {
    current_mode = MODE_USER;
    for (int i = 0; i < PERM_COUNT; i++) {
        permissions[i] = 1;
    }
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
        return permissions[perm];
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
