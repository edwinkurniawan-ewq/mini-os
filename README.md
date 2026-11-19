# 🖥️ Mini OS Project

Proyek pengembangan Sistem Operasi Mini (Mini OS) berbasis 32-bit x86/x86_64 yang berjalan di atas emulator **QEMU**. Proyek ini mencakup komponen *low-level bootloader*, *kernel CLI/shell*, *multithreading & scheduler*, *primitif sinkronisasi*, *dynamic memory allocator*, hingga *Virtual File System (VFS)* berbasis RAMDisk.

---

## 🛠️ Prasyarat & Lingkungan Pengembangan

Untuk melakukan kompilasi dan menjalankan Mini OS ini, Anda memerlukan perangkat lunak berikut:

* **Compiler C & Assembler:** `gcc` (i686-elf-gcc / gcc), `nasm`
* **Linker & Build Tool:** `gnu ld`, `make`
* **Emulator:** `qemu-system-i386` atau `qemu-system-x86_64`

---

## 🚀 Panduan Setup QEMU & Toolchain

Pilih instruksi instalasi sesuai dengan Sistem Operasi yang Anda gunakan:

### 1. Ubuntu / Debian / WSL (Windows Subsystem for Linux)
Buka terminal dan jalankan perintah:
```bash
sudo apt update
sudo apt install build-essential nasm qemu-system-x86 gcc-multilib
```

### 2. macOS (via Homebrew)
Buka terminal dan jalankan:
```bash
brew install nasm qemu make
# Jika memerlukan cross-compiler i686-elf:
brew install i686-elf-gcc
```

### 3. Windows (via MSYS2 / MinGW-w64)
1. Unduh dan pasang **MSYS2** dari [msys2.org](https://www.msys2.org/).
2. Buka terminal **MSYS2 UCRT64** dan jalankan:
   ```bash
   pacman -S gcc make nasm mingw-w64-x86_64-qemu
   ```
3. Tambahkan *path* MSYS2 ke Environment Variables Windows jika ingin menjalankannya lewat Command Prompt / PowerShell.

---

## 💻 Cara Kompilasi & Menjalankan OS

### 1. Kloning Repositori & Buka Direktori
```bash
git clone https://github.com/username/mini-os.git
cd mini-os
```

### 2. Melakukan Kompilasi & Build
Untuk mengompilasi kode sumber Assembly dan C menjadi *image kernel*:
```bash
make
```

### 3. Menjalankan Mini OS di QEMU
Untuk menyimulasikan dan membuka tampilan Mini OS (termasuk *splash logo ASCII* awal):
```bash
make run
```
*Atau jalankan perintah QEMU langsung secara manual:*
```bash
qemu-system-i386 -kernel build/mini-os.bin
# atau jika menggunakan format disk image:
qemu-system-i386 -fda build/mini-os.img
```

### 4. Membersihkan Berkas Kompilasi
Untuk menghapus berkas biner `.o` dan `.bin` hasil kompilasi sebelumnya:
```bash
make clean
```

---

## 📁 Struktur Direktori Proyek

```text
mini-os/
├── boot/                # Tahap boot & linker
│   ├── boot.asm         # Assembly entry point & bootloader
│   └── linker.ld        # Linker script untuk penataan memori kernel
├── drivers/             # Driver hardware
│   ├── keyboard.c       # Driver pembaca scancode keyboard
│   ├── timer.c          # Programmable Interval Timer (PIT)
│   └── vga.c            # Driver VGA Text Mode (80x25 display)
├── cli/                 # Modul CLI / shell
│   └── cli.c            # Parser & logika perintah CLI
├── finance/             # Modul aplikasi kas
│   └── finance.c        # CRUD transaksi keuangan
├── logbook/             # Modul buku log
│   └── logbook.c        # Pencatatan aktivitas ke RAMDisk
├── memory/              # Modul alokator memori dinamis
│   └── memory.c         # Dynamic Memory Allocator (malloc/free)
├── process/             # Modul proses: TCB, scheduler, thread
│   ├── switch.asm       # Context switching Assembly
│   ├── task.c           # TCB, Round Robin scheduler, primitif blocking
│   └── thread.c         # Demo siklus thread (CPMK 2)
├── protection/          # Modul proteksi hak akses
│   └── protect.c        # Mode USER/KERNEL, izin per fitur, audit log
├── sync/                # Modul sinkronisasi
│   └── sync.c           # Mutex (blocking) + Semaphore + deteksi deadlock
├── tests/               # Modul pengujian (test harness)
│   └── tests.c          # race condition, deadlock, semaphore, proteksi
├── vfs/                 # Modul VFS RAMDisk
│   └── vfs.c            # Virtual File System berbasis RAMDisk
├── kernel/              # Inti kernel & header bersama
│   ├── main.c           # Entry-point kernel & registrasi modul
│   ├── stubs.c          # Stub fungsi bawaan
│   └── include/         # Semua header (.h) — dipakai lintas modul
│       ├── cli.h        # Antarmuka Shell / CLI
│       ├── finance.h    # Aplikasi pencatat keuangan
│       ├── io.h         # Helper akses port I/O
│       ├── kernel.h     # Definisi umum kernel
│       ├── keyboard.h   # Antarmuka driver keyboard
│       ├── logbook.h    # Buku log aktivitas
│       ├── memory.h     # Memori alokator
│       ├── protect.h    # Proteksi & Hak Akses
│       ├── sync.h       # Mutex & Semaphore
│       ├── task.h       # TCB & Scheduler
│       ├── tests.h      # Test harness
│       ├── thread.h     # Thread lifecycle
│       ├── timer.h      # Timer PIT
│       ├── vfs.h        # Virtual File System
│       └── vga.h        # Header layar VGA
├── build/               # Hasil build (.o, .d, kernel.bin) — diabaikan Git
└── Makefile             # Automated Build System
```

---

## 👥 Pembagian Tugas & Tanggung Jawab Anggota

| Anggota Tim | Peran / Fokus Utama | Modul / File yang Dikerjakan |
| :--- | :--- | :--- |
| **Edwin** *(PIC 1)* | **Low-Level Core, Hardware Driver, CLI Kernel & Build System** | `boot/*`, `drivers/*`, `kernel/main.c`, `kernel/stubs.c`, `cli/*`, `finance/*`, `logbook/*`, `Makefile`, `cli.h`, `kernel.h`, `vga.h` |
| **Rasya** *(PIC 2)* | **Concurrency Engine (Thread, Scheduler CPU, Sinkronisasi, Deadlock & Proteksi)** | `process/switch.asm`, `process/task.c`, `sync/sync.c`, `protection/protect.c`, `tests/tests.c`, `drivers/timer.c`, `task.h`, `sync.h`, `protect.h`, `tests.h` |
| **Andika** *(PIC 3)* | **Manajemen Memori Dinamis & Virtual File System (VFS)** | `memory/memory.c`, `vfs/vfs.c`, `memory.h`, `vfs.h` |

---

## 📅 Roadmap & Progress Mingguan (Weekly Planner)

### 🔴 Minggu 1: Setup Core & Foundation (Hardware, Boot, & Allocator)
* **Edwin:** Setup repo Git, struktur folder, bootloader (`boot.asm`), `linker.ld`, GDT/IDT/ISR dasar, serta konfigurasi awal `Makefile` untuk QEMU.
* **Rasya:** Perancangan struktur data TCB (Thread Control Block), status thread, dan setup Programmable Interval Timer (`timer.c`).
* **Andika:** Perancangan header Memory Chunk (`memory.h`) dan implementasi dasar alokator memori dinamis (`memory.c`).

### 🟡 Minggu 2: Core Features (CLI, Thread Switching, & VFS)
* **Edwin:** Implementasi driver VGA Text Mode, keyboard handler, serial UART, serta pembuatan CLI internal (*splash logo ASCII* & 3 perintah inti).
* **Rasya:** Prosedur Context Switching via Assembly (`switch.asm`) dan pembangunan *Preemptive Round-Robin Scheduler*.
* **Andika:** Mekanisme *boundary checks*, mitigasi *Out of Memory* (OOM), dan perancangan node VFS RAMDisk (`vfs.h`, `vfs.c`).

### 🔵 Minggu 3: Concurrency, CRUD VFS, Sinkronisasi & Proteksi
* **Edwin:** Konsolidasi rutin `init` semua modul ke `main.c` dan *parsing* argumen CLI untuk memanggil fungsi pengujian.
* **Rasya:** ✅ *Mutex* blocking (ownership + handoff + `trylock`), *counting semaphore* (blocking wait/signal), primitif blocking task (`task_block_on`/`task_wake`), *test-harness* untuk *race condition* & *deadlock* (deteksi wait-for graph + pencegahan via resource ordering & backoff), serta skema proteksi hak akses (`protect.c`: mode USER/KERNEL, izin per fitur, audit log penolakan, critical section).
* **Andika:** Antarmuka CRUD (Create, Read, Update, Delete) berkas teks pada VFS & uji alokasi dinamis pada *payload* berkas.

**Perintah uji baru (PIC 2):** `sync` (mutex aman), `race` (race condition), `semaphore` (producer/consumer), `deadlock` (deteksi + pencegahan), `deadlock-sim` (simulasi lama), `protect` (proteksi hak akses).

### 🟢 Minggu 4: Integrasi Final, Pengujian QEMU, & Penyusunan Laporan
* **Seluruh Anggota:**
  * Pengujian *end-to-end* pada QEMU dan pembenahan *bug* (debugging).
  * Pengambilan dokumentasi log terminal dan eksekusi skenario uji.
  * Penyusunan Laporan Tugas Besar (target 10–15 halaman).
  * Finalisasi slide presentasi dan demonstrasi teknis Mini OS.


## 📜 Aturan Kolaborasi Kode

1. **File Integrator (`kernel/main.c`, `Makefile`):** Dikelola penuh oleh **Edwin**. Anggota lain hanya perlu menyediakan fungsi *initialization* modul masing-masing (misal: `memory_init()`, `task_init()`, `vfs_init()`).
2. **Header Files (`kernel/include/*.h`):** Setiap anggota memegang hak milik penuh atas header modulnya untuk mencegah terjadi *merge conflict* pada Git.
3. **Modul Pengujian (`tests/tests.c`):** Rutin pengujian yang dibuat oleh Rasya & Andika diekspos ke CLI agar bisa dipanggil melalui perintah konsol/shell.
4. **Satu Folder per Modul:** tiap modul punya folder sendiri di tingkat atas (`sync/`, `process/`, `protection/`, `tests/`, ...), inti kernel + header bersama di `kernel/`, artefak build di `build/` (diabaikan Git).