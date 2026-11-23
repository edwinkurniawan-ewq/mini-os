/*
 * kernel/rr.c -- Konfigurasi quantum dan simulator Round Robin.
 *
 * Quantum dipakai oleh timer_tick() (drivers/timer.c) sebagai batas sebelum
 * scheduler melakukan preemptive context switch. Simulator murni logika:
 * tidak mengubah task table, aman dipanggil kapan saja. Output sengaja
 * ringkas: hanya hasil akhir, tanpa log per langkah.
 */

#include "rr.h"
#include "vga.h"

/* Peta per-tick: 1 karakter = 1 tick. Dibatasi agar tidak membanjiri layar. */
#define RR_MAP_MAX  480

static uint32_t rr_quantum = RR_DEFAULT_QUANTUM;

void rr_init(void) {
    rr_quantum = RR_DEFAULT_QUANTUM;
}

uint32_t rr_get_quantum(void) {
    return rr_quantum;
}

int rr_set_quantum(uint32_t quantum) {
    if (quantum < RR_MIN_QUANTUM || quantum > RR_MAX_QUANTUM) return 0;
    rr_quantum = quantum;
    return 1;
}

static void print_uint(uint32_t value) {
    char buf[12];
    int i = 0;

    if (value == 0) {
        vga_putc('0');
        return;
    }
    while (value > 0 && i < 10) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (i > 0) {
        vga_putc(buf[--i]);
    }
}

/* ---- parser input fleksibel (tanpa libc) -------------------------------- */

typedef struct {
    const char *p;
    int len;
} rr_token_t;

static int token_is_number(const rr_token_t *t) {
    if (t->len <= 0) return 0;
    for (int i = 0; i < t->len; i++) {
        if (t->p[i] < '0' || t->p[i] > '9') return 0;
    }
    return 1;
}

static uint32_t token_uint(const rr_token_t *t, uint32_t max) {
    uint32_t value = 0;

    if (!token_is_number(t)) return 0;
    for (int i = 0; i < t->len; i++) {
        uint32_t digit = (uint32_t)(t->p[i] - '0');
        if (value > (max - digit) / 10u) return 0;
        value = value * 10u + digit;
    }
    return value;
}

static int token_colon(const rr_token_t *t) {
    for (int i = 0; i < t->len; i++) {
        if (t->p[i] == ':') return i;
    }
    return -1;
}

static void make_auto_name(char *dst, uint32_t id) {
    dst[0] = 'T';
    if (id >= 10u) {
        dst[1] = (char)('0' + (id / 10u));
        dst[2] = (char)('0' + (id % 10u));
        dst[3] = '\0';
    } else {
        dst[1] = (char)('0' + id);
        dst[2] = '\0';
    }
}

int rr_parse_tasks(const char *args, rr_task_t *tasks, int max_tasks) {
    rr_token_t tokens[12];
    int token_count = 0;
    int count = 0;
    uint32_t default_burst = RR_SIM_DEFAULT_BURST;
    int has_colon = 0, has_word = 0;
    int legacy = 0;

    if (!args || !tasks || max_tasks < 1) return 0;
    if (max_tasks > (int)RR_SIM_MAX_TASKS) max_tasks = (int)RR_SIM_MAX_TASKS;

    while (*args && token_count < (int)(sizeof(tokens) / sizeof(tokens[0]))) {
        const char *start;
        while (*args == ' ') args++;
        if (!*args) break;
        start = args;
        while (*args && *args != ' ') args++;
        tokens[token_count].p = start;
        tokens[token_count].len = (int)(args - start);
        token_count++;
    }
    if (token_count == 0) return 0;

    for (int i = 0; i < token_count; i++) {
        if (token_colon(&tokens[i]) >= 0) has_colon = 1;
        else if (!token_is_number(&tokens[i])) has_word = 1;
    }

    /* Bentuk lama: "4 12" -> T1..T4 dengan burst 12. */
    if (token_is_number(&tokens[0]) && !has_colon && !has_word) {
        uint32_t n = token_uint(&tokens[0], (uint32_t)max_tasks);
        uint32_t b = (token_count >= 2)
                   ? token_uint(&tokens[1], RR_SIM_MAX_BURST)
                   : default_burst;
        if (n < 1u || b < 1u) return 0;
        for (uint32_t i = 0; i < n; i++) {
            make_auto_name(tasks[count].name, i + 1u);
            tasks[count].burst = b;
            count++;
        }
        legacy = 1;
    }

    if (!legacy) {
        /* Angka berdiri sendiri menjadi burst default untuk task huruf. */
        for (int i = 0; i < token_count; i++) {
            if (!token_is_number(&tokens[i])) continue;
            uint32_t v = token_uint(&tokens[i], RR_SIM_MAX_BURST);
            if (v < 1u) return 0;
            default_burst = v;
        }

        for (int i = 0; i < token_count && count < max_tasks; i++) {
            rr_token_t *t = &tokens[i];

            if (token_is_number(t)) continue;

            int pos = token_colon(t);
            if (pos >= 0) {
                /* "A:150" -> nama di kiri, burst di kanan. */
                rr_token_t left  = { t->p, pos };
                rr_token_t right = { t->p + pos + 1, t->len - pos - 1 };
                uint32_t burst = token_uint(&right, RR_SIM_MAX_BURST);
                int nlen = left.len;

                if (nlen < 1 || nlen > (int)RR_SIM_NAME_LEN - 1 ||
                    burst < 1u) {
                    return 0;
                }
                for (int k = 0; k < nlen; k++) {
                    tasks[count].name[k] = left.p[k];
                }
                tasks[count].name[nlen] = '\0';
                tasks[count].burst = burst;
                count++;
            } else {
                /* "ABCD" -> tiap karakter = satu task. */
                for (int k = 0; k < t->len && count < max_tasks; k++) {
                    char c = t->p[k];
                    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                          (c >= '0' && c <= '9'))) {
                        return 0;
                    }
                    tasks[count].name[0] = c;
                    tasks[count].name[1] = '\0';
                    tasks[count].burst = default_burst;
                    count++;
                }
            }
        }
    }

    if (count < 1 || count > max_tasks) return 0;
    return count;
}

/* ---- simulasi + hasil akhir -------------------------------------------- */

static char timeline_char(const rr_task_t *t) {
    /* "A" -> 'A'; "T1" -> '1' supaya tiap task punya huruf unik. */
    if (t->name[1] != '\0') return t->name[1];
    return t->name[0];
}

static void timeline_append(char *buf, int *len, char c, uint32_t count,
                            int *truncated) {
    for (uint32_t k = 0; k < count; k++) {
        if (*len >= RR_MAP_MAX - 1) {
            *truncated = 1;
            return;
        }
        buf[(*len)++] = c;
    }
    buf[*len] = '\0';
}

void rr_run(const rr_task_t *tasks, int count) {
    uint32_t remaining[RR_SIM_MAX_TASKS];
    uint32_t turnaround[RR_SIM_MAX_TASKS];
    uint32_t elapsed = 0, total_turnaround = 0, total_waiting = 0;
    uint32_t switches = 0, done = 0;
    int current = -1;
    char timeline[RR_MAP_MAX];
    int timeline_len = 0, truncated = 0;

    if (!tasks || count < 1 || count > (int)RR_SIM_MAX_TASKS) {
        vga_set_color(0x0C);
        vga_puts("[!] Jumlah task harus 1-8.\n");
        vga_set_color(0x0F);
        return;
    }
    for (int i = 0; i < count; i++) {
        if (tasks[i].burst < 1u || tasks[i].burst > RR_SIM_MAX_BURST) {
            vga_set_color(0x0C);
            vga_puts("[!] Burst harus 1-1000 tick.\n");
            vga_set_color(0x0F);
            return;
        }
        remaining[i] = tasks[i].burst;
        turnaround[i] = 0;
    }
    timeline[0] = '\0';

    while (done < (uint32_t)count) {
        for (int i = 0; i < count; i++) {
            uint32_t slice;

            if (remaining[i] == 0u) continue;

            if (current >= 0 && current != i) {
                switches++;
            }
            current = i;

            slice = rr_quantum;
            if (slice > remaining[i]) slice = remaining[i];
            remaining[i] -= slice;
            elapsed += slice;

            /* Peta per-tick: A berjalan slice tick, lalu B, dan seterusnya. */
            timeline_append(timeline, &timeline_len,
                            timeline_char(&tasks[i]), slice, &truncated);

            if (remaining[i] == 0u) {
                done++;
                turnaround[i] = elapsed;
            }
        }
    }

    /* ---- keluaran akhir saja ---- */
    vga_set_color(0x0E);
    vga_puts("\n=== HASIL SIMULASI ROUND ROBIN ===\n");
    vga_set_color(0x0F);
    vga_puts("Quantum aktif: ");
    print_uint(rr_quantum);
    vga_puts(" tick | Task: ");
    print_uint((uint32_t)count);
    vga_puts("\n");

    vga_puts("Peta eksekusi (1 huruf = 1 tick):\n");
    int pos = 0;
    while (pos < timeline_len) {
        int take = 74;
        if (take > timeline_len - pos) take = timeline_len - pos;

        vga_puts("  ");
        for (int k = 0; k < take; k++) vga_putc(timeline[pos + k]);
        vga_putc('\n');
        pos += take;
    }
    if (truncated) {
        vga_puts("  ... (peta dipotong; total ");
        print_uint(elapsed);
        vga_puts(" tick)\n");
    }
    vga_putc('\n');

    vga_puts("  TASK   BURST   TURNAROUND   WAITING\n");
    for (int i = 0; i < count; i++) {
        uint32_t waiting = turnaround[i] - tasks[i].burst;

        total_turnaround += turnaround[i];
        total_waiting += waiting;

        int name_len = 0;
        while (tasks[i].name[name_len]) name_len++;

        vga_puts("  ");
        vga_puts(tasks[i].name);
        for (int k = name_len; k < 6; k++) vga_putc(' ');
        print_uint(tasks[i].burst);
        vga_puts("       ");
        print_uint(turnaround[i]);
        vga_puts("           ");
        print_uint(waiting);
        vga_puts("\n");
    }

    vga_set_color(0x0A);
    vga_puts("Rata-rata turnaround: ");
    print_uint(total_turnaround / (uint32_t)count);
    vga_puts(" tick | waiting: ");
    print_uint(total_waiting / (uint32_t)count);
    vga_puts(" tick | context switch: ");
    print_uint(switches);
    vga_puts("x\n");
    vga_set_color(0x0F);
    vga_puts("(Ubah quantum dengan 'quantum <tick>' lalu ulangi untuk membandingkan.)\n");
}

void rr_simulate(uint32_t task_count, uint32_t burst) {
    rr_task_t tasks[RR_SIM_MAX_TASKS];
    int count = 0;

    if (task_count < 1u || task_count > RR_SIM_MAX_TASKS ||
        burst < 1u || burst > RR_SIM_MAX_BURST) {
        vga_set_color(0x0C);
        vga_puts("[!] Parameter invalid. Rentang: task 1-8, burst 1-1000 tick.\n");
        vga_set_color(0x0F);
        return;
    }
    for (uint32_t i = 0; i < task_count; i++) {
        make_auto_name(tasks[count].name, i + 1u);
        tasks[count].burst = burst;
        count++;
    }
    rr_run(tasks, count);
}
