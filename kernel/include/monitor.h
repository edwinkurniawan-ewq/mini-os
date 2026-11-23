#ifndef MONITOR_H
#define MONITOR_H

#include <stdint.h>

/* Widget ringkas di pojok kanan atas (hanya di Kernel Mode, tombol TAB). */
void monitor_init(void);
void monitor_open(void);
void monitor_close(void);
void monitor_toggle(void);
int  monitor_is_open(void);

/* Dipanggil tiap iterasi polling keyboard: redraw bila tick PIT berubah. */
void monitor_poll(void);

/* Daftarkan aktivitas kernel sebagai baris task di tabel utama widget.
 * Status mengikuti alur task_state_t:
 *   begin -> NEW, ready -> READY, run -> RUNNING,
 *   wait  -> WAITING, end -> TERMINATED. */
void monitor_activity_begin(const char *name, uint32_t ticks_hint);
void monitor_activity_ready(void);
void monitor_activity_run(void);
void monitor_activity_wait(void);
void monitor_activity_end(uint32_t ticks);
void monitor_activity_add(const char *name, uint32_t ticks); /* langsung TERMINATED */

#endif /* MONITOR_H */
