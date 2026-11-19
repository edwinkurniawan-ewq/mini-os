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

/* Cek izin + catat penolakan ke audit log; 1 = boleh, 0 = ditolak */
int  protect_require(perm_type_t perm, const char *action);

/* Melindungi bagian kritis (interupsi dimatikan selama section) */
void protect_enter_critical(void);
void protect_exit_critical(void);

/* Audit log penolakan akses */
void protect_print_audit(void);
int  protect_denied_count(void);

/* Mengatur izin: "tambah", "lihat", "transaksi", "ubah", "hapus", "simpan" */
int protect_set_perm_by_name(const char *name, int enable);

/* Menampilkan status izin mode USER seperti di referensi */
void protect_print_perms(void);

#endif /* PROTECT_H */
