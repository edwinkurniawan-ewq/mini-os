#ifndef LOGBOOK_H
#define LOGBOOK_H

#define MAX_LOG_ENTRIES 64

typedef struct {
    int no;
    char jam[12];
    char aksi[10];
    int id;          /* -1 jika tidak ada ID */
    char tipe;       /* '+' atau '-' atau ' ' */
    char detail[40];
} log_entry_t;

void logbook_init(void);
void logbook_add(const char *aksi, int id, char tipe, const char *detail);
void logbook_print(void);
void logbook_set_logger_active(int active);
int  logbook_is_logger_active(void);
int  logbook_get_count(void);

#endif /* LOGBOOK_H */
