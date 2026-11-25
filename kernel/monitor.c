/*
 * kernel/monitor.c -- Widget WOWIT OS Thread & Memory Monitor.
 *
 * Widget ringkas 34x13 di pojok kanan atas (baris 1..13, kolom 45..78),
 * menyisakan baris 0, kolom 0..44, dan kolom 79 sebagai jarak dari teks
 * perintah. Tidak clear screen; sel di bawah widget disimpan dan dipulihkan.
 *
 * Semua aktivitas kernel (kas, thread, deadlock, quantum, test-rr, ...)
 * tampil sebagai baris task di TABEL UTAMA, tepat di bawah T0 kernel.
 * Waktu T0 memakai detik (s); waktu aktivitas memakai tick (t).
 */

#include "monitor.h"
#include "vga.h"
#include "task.h"
#include "timer.h"
#include "memory.h"
#include "vfs.h"
#include "rr.h"

#define W            VGA_WIDGET_COLS
#define ACT_MAX      6
#define ACT_NAME_LEN 10
#define TASK_ROWS    8      /* T0 kernel + 7 TCB teratas */
#define HIST_ROWS    3      /* riwayat perintah terakhir */

typedef struct {
    char     name[ACT_NAME_LEN];
    uint32_t id;
    uint32_t ticks;
    uint16_t    mem_kb;
    task_state_t state;
    uint8_t      in_use;
} activity_entry_t;

static int monitor_open_flag = 0;
static uint32_t last_second = 0;
static int activity_version = 0;
static int last_activity_version = 0;

static activity_entry_t acts[ACT_MAX];
static int activity_next = 0;
static int active_slot = -1;
static uint32_t next_activity_id = 1u;

/* ---- builder baris selebar widget (tanpa libc) --------------------------- */

static void line_clear(char *line) {
    for (int i = 0; i < W; i++) line[i] = ' ';
    line[W] = '\0';
}

static void line_text(char *line, int *col, const char *text) {
    while (*text && *col < W) line[(*col)++] = *text++;
}

static void line_text_n(char *line, int *col, const char *text, int max) {
    int n = 0;
    while (text[n] && n < max && *col < W) {
        line[(*col)++] = text[n++];
    }
}

static void line_uint(char *line, int *col, uint32_t value) {
    char buf[12];
    int i = 0;

    if (value == 0) {
        buf[i++] = '0';
    } else {
        while (value > 0 && i < 10) {
            buf[i++] = (char)('0' + (value % 10));
            value /= 10;
        }
    }
    while (i > 0) {
        if (*col >= W) break;
        line[(*col)++] = buf[--i];
    }
}

static void line_pad(char *line, int *col, int target) {
    while (*col < target && *col < W) line[(*col)++] = ' ';
}

static void row_begin(char *line, int *col) {
    line_clear(line);
    *col = 0;
    line_text(line, col, "| ");
}

static void row_end(char *line, int *col) {
    line_pad(line, col, W - 2);
    line_text(line, col, " |");
}

static void row_border(char *line) {
    line_clear(line);
    line[0] = '+';
    for (int i = 1; i < W - 1; i++) line[i] = '-';
    line[W - 1] = '+';
}

static const char *state_name(task_state_t state) {
    switch (state) {
        case TASK_NEW:        return "NEW";
        case TASK_READY:      return "READY";
        case TASK_RUNNING:    return "RUNNING";
        case TASK_WAITING:    return "WAITING";
        case TASK_TERMINATED: return "TERMINATED";
        default:              return "?";
    }
}

/* ---- gambar widget ------------------------------------------------------- */

static void draw_widget(int repaint_background) {
    char line[W + 1];
    int col;
    int shown = 0;
    int active = 0;

    if (repaint_background) {
        vga_widget_fill(VGA_WIDGET_ROW, VGA_WIDGET_COL,
                        VGA_WIDGET_ROWS, VGA_WIDGET_COLS, 0x1F);
    }

    row_border(line);
    vga_widget_write(VGA_WIDGET_ROW, VGA_WIDGET_COL, line, 0x0E);

    row_begin(line, &col);
    line_text(line, &col, "=== WOWIT OS THREAD & MEMORY MONITOR ===");
    row_end(line, &col);
    vga_widget_write(VGA_WIDGET_ROW + 1, VGA_WIDGET_COL, line, 0x0E);

    row_border(line);
    vga_widget_write(VGA_WIDGET_ROW + 2, VGA_WIDGET_COL, line, 0x0E);

    /* Jarak antar kolom minimal 2 karakter (STATUS -> MEMORI = 2 spasi). */
    row_begin(line, &col);
    line_text(line, &col, "ID");
    line_pad(line, &col, 6);
    line_text(line, &col, "NAMA");
    line_pad(line, &col, 16);
    line_text(line, &col, "STATUS");
    line_pad(line, &col, 28);
    line_text(line, &col, "MEMORI");
    line_pad(line, &col, 36);
    line_text(line, &col, "WAKTU");
    row_end(line, &col);
    vga_widget_write(VGA_WIDGET_ROW + 3, VGA_WIDGET_COL, line, 0x0F);

    /* Tabel UTAMA dibaca langsung dari TCB kernel (task.c), bukan dari
     * array aktivitas buatan. T0 = task kernel. */
    {
        tcb_t *k = task_get(0);
        const char *name = (k && k->name[0]) ? k->name : "kernel";
        const char *status = k ? state_name(k->state) : "RUNNING";
        uint32_t seconds = k ? (k->cpu_ticks / 100u) : 0u;

        if (!k || k->state != TASK_TERMINATED) active++;

        row_begin(line, &col);
        line_text(line, &col, "T0");
        line_pad(line, &col, 6);
        line_text_n(line, &col, name, 8);
        line_pad(line, &col, 16);
        line_text_n(line, &col, status, 12);
        line_pad(line, &col, 28);
        line_uint(line, &col, (uint32_t)(STACK_SIZE / 1024));
        line_text(line, &col, " KB");
        line_pad(line, &col, 36);
        line_uint(line, &col, seconds);
        line_text(line, &col, " s");
        row_end(line, &col);
        vga_widget_write(VGA_WIDGET_ROW + 4, VGA_WIDGET_COL, line, 0x1F);
        shown = 1;
    }

    for (int i = 1; i < task_total() && shown < TASK_ROWS; i++) {
        tcb_t *t = task_get(i);
        if (!t) continue;

        if (t->state != TASK_TERMINATED) active++;

        row_begin(line, &col);
        line_text(line, &col, "T");
        line_uint(line, &col, (uint32_t)t->id);
        line_pad(line, &col, 6);
        line_text_n(line, &col, t->name, 8);
        line_pad(line, &col, 16);
        line_text_n(line, &col, state_name(t->state), 12);
        line_pad(line, &col, 28);
        line_uint(line, &col, (uint32_t)(STACK_SIZE / 1024));
        line_text(line, &col, " KB");
        line_pad(line, &col, 36);
        line_uint(line, &col, t->cpu_ticks);
        line_text(line, &col, " tick");
        row_end(line, &col);
        vga_widget_write(VGA_WIDGET_ROW + 4 + shown, VGA_WIDGET_COL, line, 0x0F);
        shown++;
    }

    for (int i = shown; i < TASK_ROWS; i++) {
        row_begin(line, &col);
        row_end(line, &col);
        vga_widget_write(VGA_WIDGET_ROW + 4 + i, VGA_WIDGET_COL, line, 0x1F);
    }

    /* Pemisah tabel TCB dengan riwayat aktivitas terakhir. */
    row_border(line);
    vga_widget_write(VGA_WIDGET_ROW + 12, VGA_WIDGET_COL, line, 0x0E);

    for (int h = 0; h < HIST_ROWS; h++) {
        int idx = activity_next - HIST_ROWS + h;
        if (idx < 0) idx += ACT_MAX;

        row_begin(line, &col);
        line_text(line, &col, h == 0 ? "TERAKHIR: " : "          ");
        if (acts[idx].in_use) {
            line_text_n(line, &col, acts[idx].name, ACT_NAME_LEN - 1);
            line_pad(line, &col, 20);
            line_text_n(line, &col, state_name(acts[idx].state), 12);
            line_pad(line, &col, 38);
            line_uint(line, &col, acts[idx].ticks);
            line_text(line, &col, " tick");
        }
        row_end(line, &col);
        vga_widget_write(VGA_WIDGET_ROW + 13 + h, VGA_WIDGET_COL, line, 0x1F);
    }

    /* Footer ringkasan tetap di dasar widget. */
    row_begin(line, &col);
    line_text(line, &col, "Aktif:");
    line_uint(line, &col, (uint32_t)active);
    line_text(line, &col, " Heap:");
    line_uint(line, &col, (uint32_t)(memory_get_free() / 1024u));
    line_text(line, &col, " KB Disk:");
    line_uint(line, &col, (uint32_t)((vfs_get_total_bytes() -
                                     vfs_get_used_bytes()) / 1024u));
    line_text(line, &col, " KB");
    row_end(line, &col);
    vga_widget_write(VGA_WIDGET_ROW + 16, VGA_WIDGET_COL, line, 0x1F);

    row_border(line);
    vga_widget_write(VGA_WIDGET_ROW + 17, VGA_WIDGET_COL, line, 0x0E);
}

/* ---- API ---------------------------------------------------------------- */

void monitor_init(void) {
    monitor_open_flag = 0;
    activity_version = 0;
    last_activity_version = 0;
    activity_next = 0;
    active_slot = -1;
    next_activity_id = 1u;
    for (int i = 0; i < ACT_MAX; i++) {
        acts[i].name[0] = '\0';
        acts[i].in_use = 0;
        acts[i].state = TASK_TERMINATED;
        acts[i].ticks = 0;
        acts[i].mem_kb = 1u;
        acts[i].id = 0u;
    }
}

void monitor_open(void) {
    if (monitor_open_flag) return;
    monitor_open_flag = 1;
    last_second = timer_get_ticks() / 100u;
    last_activity_version = activity_version;
    vga_widget_enter();
    draw_widget(1);
}

void monitor_close(void) {
    if (!monitor_open_flag) return;
    monitor_open_flag = 0;
    vga_widget_leave();
}

void monitor_toggle(void) {
    if (monitor_open_flag) monitor_close();
    else monitor_open();
}

int monitor_is_open(void) {
    return monitor_open_flag;
}

void monitor_poll(void) {
    if (!monitor_open_flag) return;

    uint32_t now = timer_get_ticks();
    uint32_t second = now / 100u;

    if (second == last_second && activity_version == last_activity_version) {
        return;
    }
    last_second = second;
    last_activity_version = activity_version;
    draw_widget(0);
}

/* Ubah state entri aktif lalu paksa redraw bila widget sedang terbuka. */
static void set_active_state(task_state_t state) {
    if (active_slot < 0) return;

    activity_entry_t *e = &acts[active_slot];
    if (!e->in_use) return;

    e->state = state;
    activity_version++;
    monitor_poll();
}

void monitor_activity_begin(const char *name, uint32_t ticks_hint) {
    if (!name) return;

    int slot = activity_next;
    activity_next = (activity_next + 1) % ACT_MAX;

    int i = 0;
    while (name[i] && i < ACT_NAME_LEN - 1) {
        acts[slot].name[i] = name[i];
        i++;
    }
    acts[slot].name[i] = '\0';
    acts[slot].id = next_activity_id++;
    if (next_activity_id > 99u) next_activity_id = 1u;
    acts[slot].ticks = ticks_hint;
    acts[slot].mem_kb = 1u;
    acts[slot].state = TASK_NEW;
    acts[slot].in_use = 1u;
    active_slot = slot;

    activity_version++;
    monitor_poll();   /* tampilkan NEW segera bila widget terbuka */
}

void monitor_activity_ready(void)  { set_active_state(TASK_READY); }
void monitor_activity_run(void)    { set_active_state(TASK_RUNNING); }
void monitor_activity_wait(void)   { set_active_state(TASK_WAITING); }

void monitor_activity_end(uint32_t ticks) {
    if (active_slot < 0) return;

    activity_entry_t *e = &acts[active_slot];
    if (e->in_use) {
        e->state = TASK_TERMINATED;
        e->ticks = ticks;
    }
    active_slot = -1;

    activity_version++;
    monitor_poll();   /* update ke TERMINATED + elapsed tick */
}

void monitor_activity_add(const char *name, uint32_t ticks) {
    monitor_activity_begin(name, ticks);
    monitor_activity_ready();
    monitor_activity_run();
    monitor_activity_end(ticks);
}
