# WOWIT OS (Mini Bare-Metal x86 Operating System)

WOWIT OS adalah sistem operasi *bare-metal* 32-bit berbasis arsitektur x86 yang dibangun dari nol menggunakan bahasa C dan Assembly (NASM) tanpa bergantung pada *standard library* sistem operasi host (`stdio.h`, `stdlib.h`, `string.h`). Sistem operasi ini dirancang secara modular dengan dukungan *preemptive multitasking*, manajemen memori dinamis, sistem berkas virtual (RAMDisk), kontrol hak akses berbasis mode, serta aplikasi pengguna interaktif.

---

## 1. Struktur Direktori & Daftar File

Seluruh kode sumber dipisahkan ke dalam direktori spesifik sesuai subsistem arsitektur sistem operasi, sementara seluruh artefak hasil kompilasi (`.o`, `.d`, `kernel.exe`, `kernel.bin`) diisolasi ke dalam folder `build/`.

```text
mini-os/
├── boot/
│   ├── boot.asm             # Entry point Multiboot bootloader & inisialisasi stack 16 KB
│   └── linker.ld            # Linker script untuk tata letak segmen memori kernel (.text, .data, .bss)
├── build/                   # Direktori output hasil kompilasi (diabaikan oleh Git)
├── cli/
│   └── cli.c                # Antarmuka Command Line Interface (User Mode & Kernel Mode)
├── drivers/
│   ├── keyboard.c           # Driver PS/2 Keyboard (polling) & IntelliMouse (scroll wheel)
│   ├── timer.c              # Driver Programmable Interval Timer (PIT) & penghitung tick preemptive
│   └── vga.c                # Driver VGA Text Mode (0xB8000), scrollback buffer 200 baris, & overlay UI
├── finance/
│   └── finance.c            # Sub-shell Catatan Keuangan Pribadi (WOWIT Kas, CRUD) terintegrasi VFS & ACL
├── kernel/
│   ├── include/             # Seluruh file header (.h) untuk antarmuka antar-modul kernel
│   ├── main.c               # Fungsi utama inisialisasi seluruh subsistem kernel (_kernel_main)
│   ├── rr.c                 # Mesin simulasi penjadwalan Round Robin dinamis
│   └── stubs.c              # Implementasi GDT, IDT, dan remapping PIC 8259 untuk interupsi hardware
├── logbook/
│   └── logbook.c            # Modul pencatatan aktivitas transaksi keuangan
├── memory/
│   └── memory.c             # Manajemen heap dinamis 64 KB (kmalloc/kfree, first-fit, splitting, coalescing)
├── process/
│   ├── switch.asm           # Rutin Assembly untuk low-level context switching antar-thread
│   ├── task.c               # Manajemen Thread Control Block (TCB) & algoritma Round Robin scheduler
│   └── thread.c             # Demonstrasi siklus hidup thread (NEW, READY, RUNNING, WAITING, TERMINATED)
├── protection/
│   └── protect.c            # Kontrol hak akses berbasis mode (USER/KERNEL), critical section, & audit log
├── sync/
│   └── sync.c               # Primitif sinkronisasi atomik (Mutex, Counting Semaphore) & deteksi Deadlock
├── tests/
│   └── tests.c              # Kumpulan test harness untuk verifikasi seluruh subsistem OS
├── vfs/
│   └── vfs.c                # Virtual File System berbasis RAMDisk (8 file x 8 KB)
├── .gitignore               # Konfigurasi pengecualian file build dari repositori Git
└── Makefile                 # Otomatisasi kompilasi GCC, NASM, linking, dan eksekusi QEMU
```

---

## 2. Arsitektur Hak Akses (User Mode vs Kernel Mode)

WOWIT OS menerapkan pemisahan lingkungan eksekusi menjadi dua tingkat hak akses untuk menjaga keamanan dan stabilitas sistem:

### A. User Mode (`wowit-os>`)
* **Status:** Mode bawaan (*default*) saat sistem pertama kali melakukan *booting*.
* **Batasan:** Pengguna hanya dapat mengakses perintah dasar dan aplikasi pengguna. Akses ke pengujian sistem tingkat rendah, konfigurasi penjadwalan CPU, dan monitor thread dikunci.
* **Proteksi Aplikasi:** Operasi sensitif (seperti penghapusan data pada aplikasi keuangan) dapat dibatasi oleh kebijakan izin (*Access Control List*) yang diatur dari Kernel Mode.

### B. Kernel Mode (`wowit-kernel#`)
* **Cara Akses:** Ketikkan perintah tersembunyi `kernel mode` pada prompt User Mode (`wowit-os>`). Perintah ini sengaja tidak ditampilkan pada menu `help` di User Mode.
* **Autentikasi:** Sistem akan meminta kata sandi administrator (**Password default:** `wowit123`). Karakter yang diketik disensor menggunakan tanda bintang (`*`).
* **Keamanan Percobaan:** Pengguna memiliki maksimal **3 kali percobaan** memasukkan password. Jika gagal sebanyak 3 kali berturut-turut, sistem memblokir akses dan mengembalikan pengguna secara paksa ke User Mode.
* **Monitor Real-Time (`TAB`):** Saat berada di dalam Kernel Mode, menekan tombol **`TAB`** kapan saja akan menampilkan widget *overlay* **WOWIT OS Monitor** di pojok kanan atas layar secara *real-time* (menampilkan daftar TCB aktif, status, penggunaan memori dalam `KB`, waktu berjalan dalam detik/tick, serta sisa kapasitas Heap dan RAMDisk). Tekan **`TAB`** atau **`ESC`** kembali untuk menutup widget.

---

## 3. Daftar Perintah & Panduan Penggunaan

### A. Perintah User Mode (`wowit-os>`)
Perintah-perintah ini tersedia di User Mode dan tetap dapat dijalankan saat berada di dalam Kernel Mode:

| Perintah | Deskripsi & Cara Penggunaan |
| :--- | :--- |
| `info` | Menampilkan informasi identitas sistem operasi, arsitektur, dan informasi pengembang. |
| `kas` | Membuka sub-shell **Aplikasi Pencatat Keuangan** (`wowit-os/kas>`). |
| `help` | Menampilkan daftar perintah yang tersedia pada mode saat ini. |
| `exit` | Menghentikan sesi shell (atau keluar dari Kernel Mode kembali ke User Mode). |
| `kernel mode` | *(Perintah Rahasia)* Meminta autentikasi password untuk masuk ke lingkungan administrator **Kernel Mode**. |

#### Perintah di dalam Sub-Shell Aplikasi Keuangan (`wowit-os/kas>`)
| Perintah | Format & Contoh Penggunaan | Fungsi |
| :--- | :--- | :--- |
| `lihat` | `lihat` | Menampilkan seluruh daftar transaksi pemasukan, pengeluaran, dan total saldo. |
| `tambah` | `tambah + 50000 Gaji` atau `tambah - 15000 Makan` | Mencatat transaksi pemasukan (`+`) atau pengeluaran (`-`) baru. |
| `ubah` | `ubah 1 75000` | Mengubah nominal transaksi berdasarkan nomor ID. |
| `hapus` | `hapus 1` | Menghapus transaksi berdasarkan nomor ID (tunduk pada aturan izin proteksi). |
| `simpan` | `simpan` | Menyimpan data buku kas secara persisten ke berkas `kas.dat` di RAMDisk VFS. |
| `6` | `6` | Ekspor ringkasan pemasukan, pengeluaran, dan saldo akhir ke `laporan_kas.txt` di RAMDisk VFS. |
| `exit` | `exit` | Keluar dari sub-shell aplikasi keuangan kembali ke shell utama. |

Seluruh operasi di atas dilindungi `kas_mutex`. Data transaksi dan saldo pribadi juga dipakai oleh uji `race`/`sync`, serta oleh dua resource `mutex_buku_kas` (tabel transaksi pribadi di RAM) dan `mutex_file_vfs` (`kas.dat`) pada perintah `deadlock`. |

---

### B. Perintah Khusus Kernel Mode (`wowit-kernel#`)
Perintah berikut hanya dapat diakses setelah berhasil masuk ke **Kernel Mode**:

#### 1. Manajemen Proses, Thread, & Penjadwalan CPU
| Perintah | Format & Contoh Penggunaan | Fungsi |
| :--- | :--- | :--- |
| `quantum` | `quantum` atau `quantum 10` | Melihat atau mengubah nilai *time quantum* Round Robin (1–1000 tick). |
| `test-rr` | `test-rr AB` atau `test-rr A:150 B:80` | Menjalankan pengujian penjadwalan Round Robin dengan input subjek fleksibel sesuai quantum aktif. |
| `rr-sim` | `rr-sim` | Menjalankan simulasi cepat Round Robin standar (4 task dengan burst 8 tick). |
| `preempt-test` | `preempt-test` | Menguji *preemptive multitasking* nyata menggunakan interupsi timer hardware IRQ0 tanpa `yield` manual. |
| `thread` | `thread` | Mendemonstrasikan siklus hidup thread dari state `NEW` hingga `TERMINATED` pada TCB asli. |
| `threads` | `threads` | Menampilkan daftar seluruh thread yang terdaftar di tabel TCB kernel. |
| `sched` | `sched` | Menguji mekanisme perpindahan konteks (*context switching*) antar-thread pada scheduler. |
| `laporan` | `laporan` | Menjalankan pembuatan laporan keuangan secara konkuren oleh 2 thread terpisah. |

#### 2. Sinkronisasi & Penanganan Deadlock
| Perintah | Format & Contoh Penggunaan | Fungsi |
| :--- | :--- | :--- |
| `race` | `race` | Thread Catat Transaksi dan Thread Auto-Backup / Pemasukan Rutin menambah saldo pribadi tanpa `kas_mutex` → *lost update* nyata. |
| `sync` | `sync` | Kedua thread pencatat keuangan pribadi menambah saldo dengan `kas_mutex` → saldo akhir akurat 100%. |
| `semaphore` | `semaphore` | Menguji sinkronisasi akses sumber daya terbatas menggunakan *counting semaphore*. |
| `deadlock` | `deadlock` | Thread Catat Transaksi (`mutex_buku_kas` → `mutex_file_vfs`) vs Thread Auto-Backup VFS (sebaliknya), deteksi *wait-for graph*, lalu pemulihan dengan *resource ordering* (alternatif: *trylock + backoff*). |

#### 3. Manajemen Memori, Sistem Berkas (VFS), & Proteksi
| Perintah | Format & Contoh Penggunaan | Fungsi |
| :--- | :--- | :--- |
| `memtest` | `memtest` | Menguji alokasi/dealokasi heap (`kmalloc`/`kfree`), pengujian *Out of Memory* (OOM), dan *coalescing*. |
| `vfs` | `vfs` | Menjalankan pengujian otomatis siklus lengkap VFS (`create` $\rightarrow$ `read` $\rightarrow$ `update` $\rightarrow$ `delete`). |
| `ls` | `ls` | Menampilkan daftar seluruh berkas yang tersimpan di dalam RAMDisk VFS beserta ukurannya. |
| `touch` | `touch catatan.txt Halo_WOWIT` | Membuat atau memperbarui isi berkas di dalam RAMDisk VFS secara interaktif. |
| `cat` | `cat catatan.txt` | Membaca dan mencetak isi berkas dari dalam RAMDisk VFS. |
| `rm` | `rm catatan.txt` | Menghapus berkas dari dalam RAMDisk VFS. |
| `protect` | `protect` | Menguji penegakan kontrol hak akses antara mode `USER` dan `KERNEL` beserta pencatatan *audit log*. |
| `izin` | `izin` atau `izin hapus off` | Mengatur izin akses fitur aplikasi secara dinamis (contoh: melarang aksi `hapus` di User Mode). |
| `log` | `log` | Menampilkan buku log aktivitas transaksi yang tercatat di kernel. |

#### 4. Utilitas Sistem & Navigasi Layar
| Perintah / Tombol | Fungsi |
| :--- | :--- |
| `clear` | Membersihkan seluruh tampilan dan riwayat layar terminal VGA. |
| `echo <teks>` | Mencetak kembali teks argumen ke layar terminal. |
| `reboot` | Melakukan *restart* sistem operasi secara penuh. |
| `logout` | Keluar dari Kernel Mode dan kembali ke User Mode (`wowit-os>`). |
| Tombol `TAB` | Membuka/menutup widget *real-time* **Thread & Memory Monitor** di pojok kanan atas (khusus Kernel Mode). |
| `Page Up` / `Page Down` | Menggulir riwayat tampilan layar ke atas atau ke bawah (hingga 200 baris riwayat). |
| *Mouse Scroll Wheel* | Menggulir tampilan layar ke atas atau ke bawah secara langsung menggunakan roda mouse / trackpad PS/2. |

---

## 4. Cara Kompilasi & Menjalankan di QEMU

Pastikan toolchain `gcc` (i386/multilib), `nasm`, `ld`, `objcopy`, dan `qemu-system-i386` sudah terpasang di lingkungan MSYS2 / Linux Anda:

```bash
# Membersihkan artefak build sebelumnya, mengompilasi ulang seluruh modul, dan menjalankan QEMU
make clean && make run
```

---

## 5. Dokumentasi Penggunaan Generative AI

Dalam proses pengembangan WOWIT OS, perangkat **Generative AI (Google Gemini & OpenCode)** dimanfaatkan sebagai asisten pengembangan dengan batasan dan tanggung jawab berikut:
1. **Perancangan Arsitektur & Modularisasi:** Membantu menyusun pemisahan struktur direktori subsistem kernel, perancangan antarmuka pemisahan User/Kernel Mode, serta desain tata letak widget monitor berbasis VGA Text Mode.
2. **Debugging Bare-Metal x86:** Membantu menganalisis konvensi pemanggilan fungsi Assembly-C, remapping PIC 8259 agar interupsi timer (IRQ0) tidak berbenturan dengan *polling* PS/2 keyboard/mouse, serta pencegahan *race condition* pada primitif sinkronisasi.
3. **Validasi & Pengujian Manual:** Seluruh kode yang dihasilkan telah diaudit, dikompilasi ulang tanpa *warning*, dan diuji fungsionalitasnya secara langsung oleh tim pengembang melalui emulator QEMU.