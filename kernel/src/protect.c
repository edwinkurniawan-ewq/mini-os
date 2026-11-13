#include "protect.h"
#include "kernel.h"

static privilege_level_t current_level = LEVEL_KERNEL;

/*
 * Simple access control:
 *   - LEVEL_KERNEL caller can access everything
 *   - LEVEL_USER   caller denied access to LEVEL_KERNEL resources
 * Week 2: extend with GDT CPL rings / page-fault-based enforcement
 */
int protect_check(privilege_level_t required, privilege_level_t caller) {
    if (required == LEVEL_KERNEL && caller == LEVEL_USER) return 0;
    return 1;
}

privilege_level_t protect_get_level(void) {
    return current_level;
}

void protect_init(void) {
    current_level = LEVEL_KERNEL;
    vga_puts("[PROTECT] Protection initialized (kernel mode)\n");
}
