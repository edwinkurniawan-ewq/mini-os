#ifndef PROTECT_H
#define PROTECT_H

typedef enum {
    LEVEL_KERNEL = 0,
    LEVEL_USER   = 1
} privilege_level_t;

/* Returns 1 if access granted, 0 if denied */
int  protect_check(privilege_level_t required, privilege_level_t caller);
void protect_init(void);

#endif /* PROTECT_H */
