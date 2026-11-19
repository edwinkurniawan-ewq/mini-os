#ifndef PROTECT_H
#define PROTECT_H

typedef enum {
    MODE_USER = 0,
    MODE_KERNEL = 1
} os_mode_t;

typedef enum {
    PERM_TAMBAH = 0,
    PERM_LIHAT  = 1,
    PERM_UBAH   = 2,
    PERM_HAPUS  = 3,
    PERM_SIMPAN = 4,
    PERM_COUNT  = 5
} perm_type_t;

void protect_init(void);
os_mode_t protect_get_mode(void);
void protect_set_mode(os_mode_t mode);

/* Mengembalikan 1 jika diizinkan, 0 jika ditolak */
int protect_check_perm(perm_type_t perm);

/* Mengatur izin: "tambah", "lihat", "transaksi", "ubah", "hapus", "simpan" */
int protect_set_perm_by_name(const char *name, int enable);

/* Menampilkan status izin mode USER seperti di referensi */
void protect_print_perms(void);

#endif /* PROTECT_H */
