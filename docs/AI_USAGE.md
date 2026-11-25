# Dokumentasi Penggunaan Generative AI — WOWIT OS

Dokumen ini memenuhi catatan pada `requirement.txt` bahwa penggunaan bantuan
Generative AI **wajib didokumentasikan**. Seluruh kode tetap ditinjau, diuji,
dan disetujui oleh tim pengembang sebelum dimasukkan ke repositori.

## 1. Informasi Umum

| Item | Keterangan |
| :--- | :--- |
| Proyek | WOWIT OS — kernel x86 32-bit bare-metal untuk Tugas Besar Sistem Operasi |
| Ruang lingkup AI | Perancangan arsitektur modul, debugging bare-metal x86, pembuatan test harness, dan validasi manual |
| Batasan | AI tidak menggantikan verifikasi tim; semua keluaran dibaca baris per baris dan diuji di QEMU |
| Toolchain | GCC `-m32 -ffreestanding`, NASM `-f win32`, `ld -m i386pe`, `objcopy`, QEMU `qemu-system-i386` |

## 2. Area yang Dibantu AI

### 2.1 Perancangan arsitektur modul
- Memisahkan kernel menjadi folder top-level: `boot/`, `drivers/`, `kernel/`, `cli/`, `process/`, `sync/`, `protection/`, `memory/`, `vfs/`, `finance/`, `logbook/`, `tests/`.
- Menentukan kontrak antarmuka lewat header di `kernel/include/` sehingga tiap anggota bisa mengerjakan modulnya tanpa konflik.
- Merancang alur inisialisasi di `kernel/main.c`: GDT → IDT/PIC → driver → allocator → VFS → task → CLI → `sti`.

### 2.2 Debugging bare-metal x86
- Menyusun GDT 3 entri (null, kernel code `0x08`, kernel data `0x10`) beserta `lgdt` dan flush segment register di `boot/flush.asm`.
- Merancang 256 entri IDT + stub Assembly `boot/isr.asm`, remap PIC 8259 ke vektor 32–47, dan EOI sebelum context switch.
- Menangani urutan aman anti-*triple fault*: primitive `cli`/`popfl` disiapkan lebih dulu di `kernel/include/irq.h`, `sync/sync.c`, dan `protection/protect.c` sebelum `sti` di `kernel_main()`.
- Menyusun interrupt frame buatan di `task_create_pending()` agar `context_switch()` me-`ret` ke `_isr_restore` dan `iretd` masuk ke entry thread.
- Memastikan IRQ1/IRQ12 tetap di-*mask* karena keyboard/mouse dibaca lewat polling port `0x60`/`0x64`.

### 2.3 Pembuatan test harness
- `tests/tests.c`: uji `race` vs `sync`, deadlock + deteksi wait-for graph, semaphore, proteksi hak akses, alokasi/OOM/coalescing, dan siklus CRUD RAMDisk.
- `process/task.c`: perintah `preempt-test` dengan dua thread CPU-bound tanpa `task_yield()` manual untuk membuktikan preemption IRQ0.
- Perbaikan demo agar status dibaca dari `tcb->state` sungguhan, bukan string hardcode.

### 2.4 Validasi manual
- Membantu merancang skenario pengujian QEMU dan daftar perintah demonstrasi.
- Membantu menafsirkan hasil build (`0 error, 0 warning`) dan log eksekusi.
- Semua keputusan akhir—termasuk pilihan pendekatan proteksi (ACL sederhana, bukan paging)—dibuat oleh tim.

## 3. Verifikasi oleh Tim

Sebelum perubahan dianggap selesai, tim melakukan:

1. `make clean && make` → 0 error, 0 warning.
2. Boot QEMU dan konfirmasi tidak ada *triple fault*.
3. Menjalankan seluruh perintah uji pada shell kernel dan mencocokkan keluaran dengan ekspektasi.
4. Meninjau ulang kode yang dihasilkan AI untuk memastikan tidak ada library C standar yang dipakai.

## 4. Catatan Batasan

- AI tidak memiliki akses langsung ke mesin penguji; seluruh klaim fungsional dibuktikan lewat build dan demonstrasi QEMU oleh tim.
- Komentar kode yang menjelaskan alasan teknis (mis. EOI sebelum context switch) ditulis ulang dengan bahasa tim agar mudah dipertanggungjawabkan.
- Tidak ada kode yang dimasukkan hanya karena "terlihat benar"; setiap blok harus bisa dijelaskan oleh anggota yang bertugas.
