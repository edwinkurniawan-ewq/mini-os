#ifndef VFS_H
#define VFS_H

#include <stddef.h>
#include <stdint.h>

#define VFS_MAX_FILES 8
#define VFS_MAX_FILENAME 16
#define VFS_FILE_SIZE 8192

typedef struct {
    char name[VFS_MAX_FILENAME];
    uint8_t data[VFS_FILE_SIZE];
    size_t size;
    int used;
} vfs_file_t;

void   vfs_init(void);
int    vfs_write(const char *name, const void *buf, size_t size);
int    vfs_read(const char *name, void *buf, size_t max_size);
int    vfs_exists(const char *name);
size_t vfs_get_size(const char *name);
int    vfs_delete(const char *name);
void   vfs_list(void);

/* Informasi kapasitas RAMDisk (dipakai monitor & pengujian). */
size_t vfs_get_total_bytes(void);
size_t vfs_get_used_bytes(void);
int    vfs_get_used_files(void);

#endif /* VFS_H */
