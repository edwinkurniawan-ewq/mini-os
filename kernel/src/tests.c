#include "task.h"
#include "sync.h"
#include "kernel.h"

/*
 * Test harness — functions exposed via CLI commands (registered by Edwin)
 *
 * Week 1:  test_tasks()   — create demo threads, print TCB table
 * Week 3:  test_race()    — shared counter with/without mutex
 *          test_deadlock() — two threads, two mutexes, circular wait + fix
 * Week 3:  test_malloc()  — alloc/free/OOM demo
 *          test_vfs()     — CRUD on RAMDisk
 */

/* ----- demo thread entry points (live when scheduler active, Week 2) ----- */

static void task_a(void) { for (;;) { __asm__ volatile ("hlt"); } }
static void task_b(void) { for (;;) { __asm__ volatile ("hlt"); } }

/* ----- Week 1 ------------------------------------------------------------ */

void test_tasks(void) {
    vga_puts("[TEST] Membuat demo threads...\n");
    task_create("TaskA", task_a);
    task_create("TaskB", task_b);
    task_print_all();
}

/* ----- Week 3 stubs ------------------------------------------------------- */

void test_race(void)     { vga_puts("[TEST] test_race: diimplementasi Week 3\n"); }
void test_deadlock(void) { vga_puts("[TEST] test_deadlock: diimplementasi Week 3\n"); }

void test_malloc(void)   { vga_puts("[TEST] test_malloc: diimplementasi Week 3\n"); }
void test_vfs(void)      { vga_puts("[TEST] test_vfs: diimplementasi Week 3\n"); }
