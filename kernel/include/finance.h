#ifndef FINANCE_H
#define FINANCE_H

#include "sync.h"

/* Nominal pemasukan pribadi yang dipakai uji race/sync pada data kas sungguhan. */
#define KAS_TEST_TOPUP 50000

/* Kunci Catatan Keuangan Pribadi (WOWIT Kas): satu untuk buku transaksi,
 * satu lagi untuk berkas kas.dat di RAMDisk VFS. */
extern mutex_t kas_mutex;
extern mutex_t mutex_buku_kas;
extern mutex_t mutex_file_vfs;

void finance_init(void);
void finance_run(void);
void finance_get_summary(int *out_count, int *out_saldo);
int  finance_save_to_ramdisk(void);
int  finance_load_from_ramdisk(void);

/* ---- Antarmuka uji: memakai buku kas pribadi, bukan counter abstrak ---- */
void finance_test_snapshot(void);
void finance_test_reset(void);
int  finance_test_count(void);
int  finance_test_saldo(void);
void finance_test_restore(void);

/* Thread Catat Transaksi dan Thread Auto-Backup / Pemasukan Rutin. */
void finance_catat_race_worker(void);
void finance_backup_race_worker(void);
void finance_catat_sync_worker(void);
void finance_backup_sync_worker(void);

/* ---- Skenario deadlock dua resource pada catatan keuangan pribadi ---- */
void finance_deadlock_prepare(void);
void finance_deadlock_catat_unsafe(void);
void finance_deadlock_backup_unsafe(void);
void finance_deadlock_catat_ordered(void);
void finance_deadlock_backup_ordered(void);
void finance_deadlock_catat_trylock(void);
void finance_deadlock_backup_trylock(void);

#endif /* FINANCE_H */
