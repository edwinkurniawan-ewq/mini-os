#ifndef RR_H
#define RR_H

#include <stdint.h>

/* Round Robin: quantum dalam tick PIT (1 tick = 10 ms). */
#define RR_DEFAULT_QUANTUM 5u
#define RR_MIN_QUANTUM     1u
#define RR_MAX_QUANTUM     1000u
#define RR_SIM_MAX_TASKS   8u
#define RR_SIM_MAX_BURST   1000u
#define RR_SIM_DEFAULT_BURST 100u
#define RR_SIM_NAME_LEN    4u

typedef struct {
    char     name[RR_SIM_NAME_LEN];
    uint32_t burst;
} rr_task_t;

void     rr_init(void);
uint32_t rr_get_quantum(void);
int      rr_set_quantum(uint32_t quantum);   /* 1 = sukses, 0 = nilai di luar rentang */

/* Parse input bebas: "AB", "ABCD", "A:150 B:80", "AB 120", "4 12".
 * Mengembalikan jumlah task, atau 0 bila format/parameter tidak valid. */
int      rr_parse_tasks(const char *args, rr_task_t *tasks, int max_tasks);

/* Jalankan simulasi dan cetak HANYA hasil akhir (tanpa log per langkah). */
void     rr_run(const rr_task_t *tasks, int count);

/* Kompatibilitas rr-sim: bangun T1..Tn lalu panggil rr_run(). */
void     rr_simulate(uint32_t task_count, uint32_t burst);

#endif /* RR_H */
