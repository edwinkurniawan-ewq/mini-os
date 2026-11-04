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
├── boot/
│   ├── boot.asm        # Assembly entry point & bootloader
│   └── linker.ld       # Linker script untuk penataan memori kernel
├── drivers/
│   ├── keyboard.c      # Driver pembaca scancode keyboard
│   ├── serial.c        # Output serial UART untuk debugging
│   └── vga.c           # Driver VGA Text Mode (80x25 display)
├── kernel/
│   ├── include/        # Berkas Header (.h)
│   │   ├── cli.h       # Antarmuka Shell / CLI
│   │   ├── kernel.h    # Definisi umum kernel
│   │   ├── memory.h    # Memori alokator
│   │   ├── protect.h   # Proteksi & Hak Akses
│   │   ├── sync.h      # Mutex & Semaphore
│   │   ├── task.h      # TCB & Scheduler
│   │   ├── vga.h       # Header layar VGA
│   │   └── vfs.h       # Virtual File System
│   └── src/            # Implementasi Kode C (.c) & Assembly (.asm)
│       ├── cli.c       # Parser & logika perintah CLI
│       ├── main.c      # Entry-point kernel & registrasi modul
│       ├── memory.c    # Dynamic Memory Allocator (malloc/free)
│       ├── protect.c   # Skenario proteksi memori / user mode
│       ├── stubs.c     # Stub fungsi bawaan
│       ├── switch.asm  # Context switching Assembly
│       ├── sync.c      # Sinkronisasi & mitigasi deadlock
│       ├── task.c      # Thread manager & Round Robin scheduler
│       ├── tests.c     # Pengujian (race condition, deadlock, vfs)
│       └── vfs.c       # RAMDisk Virtual File System
└── Makefile            # Automated Build System
```

---

## 👥 Pembagian Tugas & Tanggung Jawab Anggota

| Anggota Tim | Peran / Fokus Utama | Modul / File yang Dikerjakan |
| :--- | :--- | :--- |
| **Edwin** *(PIC 1)* | **Low-Level Core, Hardware Driver, CLI Kernel & Build System** | `boot/*`, `drivers/*`, `kernel/src/cli.c`, `kernel/src/main.c`, `kernel/src/stubs.c`, `Makefile`, `cli.h`, `kernel.h`, `vga.h` |
| **Rasya** *(PIC 2)* | **Concurrency Engine (Thread, Scheduler CPU, Sinkronisasi, Deadlock & Proteksi)** | `arch/switch.asm`, `drivers/timer.c`, `kernel/src/task.c`, `kernel/src/sync.c`, `kernel/src/protect.c`, `kernel/src/tests.c`, `task.h`, `sync.h`, `protect.h` |
| **Andika** *(PIC 3)* | **Manajemen Memori Dinamis & Virtual File System (VFS)** | `kernel/src/memory.c`, `kernel/src/vfs.c`, `memory.h`, `vfs.h` |

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
* **Rasya:** Implementasi *Mutex*, *Semaphore*, *test-harness* untuk *race condition* & *deadlock*, serta skema proteksi hak akses (`protect.c`).
* **Andika:** Antarmuka CRUD (Create, Read, Update, Delete) berkas teks pada VFS & uji alokasi dinamis pada *payload* berkas.

### 🟢 Minggu 4: Integrasi Final, Pengujian QEMU, & Penyusunan Laporan
* **Seluruh Anggota:**
  * Pengujian *end-to-end* pada QEMU dan pembenahan *bug* (debugging).
  * Pengambilan dokumentasi log terminal dan eksekusi skenario uji.
  * Penyusunan Laporan Tugas Besar (target 10–15 halaman).
  * Finalisasi slide presentasi dan demonstrasi teknis Mini OS.

---

## 📜 Aturan Kolaborasi Kode

1. **File Integrator (`kernel/src/main.c`, `Makefile`):** Dikelola penuh oleh **Edwin**. Anggota lain hanya perlu menyediakan fungsi *initialization* modul masing-masing (misal: `memory_init()`, `task_init()`, `vfs_init()`).
2. **Header Files (`kernel/include/*.h`):** Setiap anggota memegang hak milik penuh atas header modulnya untuk mencegah terjadi *merge conflict* pada Git.
3. **Modul Pengujian (`kernel/src/tests.c`):** Rutin pengujian yang dibuat oleh Rasya & Andika diekspos ke CLI agar bisa dipanggil melalui perintah konsol/shell.