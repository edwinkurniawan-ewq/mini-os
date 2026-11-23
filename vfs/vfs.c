#include "vfs.h"
#include "kernel.h"

static vfs_file_t ramdisk_files[VFS_MAX_FILES];

static int v_streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void v_strcpy(char *dst, const char *src, int max) {
    int i = 0;
    while (i < max - 1 && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void vfs_init(void) {
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        ramdisk_files[i].used = 0;
        ramdisk_files[i].size = 0;
        ramdisk_files[i].name[0] = '\0';
    }
}

int vfs_write(const char *name, const void *buf, size_t size) {
    if (!name || !buf || size > VFS_FILE_SIZE) return -1;

    /* Cari file yang sudah ada (UPDATE) */
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (ramdisk_files[i].used && v_streq(ramdisk_files[i].name, name)) {
            const uint8_t *src = (const uint8_t *)buf;
            for (size_t s = 0; s < size; s++) {
                ramdisk_files[i].data[s] = src[s];
            }
            ramdisk_files[i].size = size;
            return 0;
        }
    }

    /* Cari slot kosong (CREATE) */
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (!ramdisk_files[i].used) {
            v_strcpy(ramdisk_files[i].name, name, VFS_MAX_FILENAME);
            const uint8_t *src = (const uint8_t *)buf;
            for (size_t s = 0; s < size; s++) {
                ramdisk_files[i].data[s] = src[s];
            }
            ramdisk_files[i].size = size;
            ramdisk_files[i].used = 1;
            return 0;
        }
    }

    return -1;
}

int vfs_read(const char *name, void *buf, size_t max_size) {
    if (!name || !buf) return -1;

    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (ramdisk_files[i].used && v_streq(ramdisk_files[i].name, name)) {
            size_t copy_len = ramdisk_files[i].size;
            if (copy_len > max_size) copy_len = max_size;
            uint8_t *dst = (uint8_t *)buf;
            for (size_t s = 0; s < copy_len; s++) {
                dst[s] = ramdisk_files[i].data[s];
            }
            return (int)copy_len;
        }
    }
    return -1;
}

int vfs_exists(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (ramdisk_files[i].used && v_streq(ramdisk_files[i].name, name)) {
            return 1;
        }
    }
    return 0;
}

size_t vfs_get_size(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (ramdisk_files[i].used && v_streq(ramdisk_files[i].name, name)) {
            return ramdisk_files[i].size;
        }
    }
    return 0;
}

int vfs_delete(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (ramdisk_files[i].used && v_streq(ramdisk_files[i].name, name)) {
            ramdisk_files[i].used = 0;
            ramdisk_files[i].size = 0;
            ramdisk_files[i].name[0] = '\0';
            return 0;
        }
    }
    return -1;
}

void vfs_list(void) {
    vga_set_color(0x0C);
    vga_puts("BERKAS RAMDISK (VFS):\n");
    vga_set_color(0x0F);
    int ada = 0;
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (ramdisk_files[i].used) {
            ada = 1;
            vga_puts(" - ");
            vga_puts(ramdisk_files[i].name);
            vga_puts(" (");
            char buf[12];
            int sz = (int)ramdisk_files[i].size;
            int bi = 0;
            if (sz == 0) buf[bi++] = '0';
            else {
                char tmp[12]; int ti = 0;
                while (sz > 0) { tmp[ti++] = (sz % 10) + '0'; sz /= 10; }
                while (ti > 0) buf[bi++] = tmp[--ti];
            }
            buf[bi] = '\0';
            vga_puts(buf);
            vga_puts(" B)\n");
        }
    }
    if (!ada) {
        vga_puts("(RAMDisk kosong)\n");
    }
}

size_t vfs_get_total_bytes(void) {
    return (size_t)VFS_MAX_FILES * (size_t)VFS_FILE_SIZE;
}

size_t vfs_get_used_bytes(void) {
    size_t used = 0;
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (ramdisk_files[i].used) used += ramdisk_files[i].size;
    }
    return used;
}

int vfs_get_used_files(void) {
    int count = 0;
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (ramdisk_files[i].used) count++;
    }
    return count;
}
