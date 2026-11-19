#ifndef FINANCE_H
#define FINANCE_H

void finance_init(void);
void finance_run(void);
void finance_get_summary(int *out_count, int *out_saldo);
int  finance_save_to_ramdisk(void);
int  finance_load_from_ramdisk(void);

#endif /* FINANCE_H */
