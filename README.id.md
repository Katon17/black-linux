# Black (Xbox, 2006) — Port Native Linux untuk Linux Mint 22.3

[English](README.md) | **Bahasa Indonesia**

Port **native Linux x86_64** untuk *Black* versi Xbox original. Dibuat dengan **static recompilation**: kode x86 game diterjemahkan ke C++, lalu dikompilasi jadi program Linux biasa (ELF).

**Bukan** emulator, **bukan** Wine/Proton, **bukan** Electron atau web engine. Grafisnya pakai **Vulkan** (Mesa ANV untuk Intel HD 620), sedangkan window, audio, keyboard, mouse, dan controller lewat **SDL3**.

> ⚠️ **Paket ini TIDAK berisi kode game maupun aset game.** Kamu harus pakai copy game milikmu sendiri (USA atau Europe), entah file `.iso` atau folder hasil ekstrak yang berisi `default.xbe`. Black adalah © Criterion Games / Electronic Arts. Ini proyek tidak resmi.

**Download:** ambil `black-linux_<versi>_amd64.deb` di halaman [Releases](https://github.com/Katon17/black-linux/releases).

**Diuji:** ThinkPad X270 (i5 gen 7, Intel HD 620, 8 GB RAM, Linux Mint 22.3 XFCE) dengan grafik rata kanan: **90–100+ FPS**.

---

## 1. Install (di ThinkPad X270)

```bash
# download .deb dari halaman Releases dulu
sudo apt install ./black-linux_0.1.2_amd64.deb
```

`apt` otomatis menginstal dependensi yang dibutuhkan: `clang`, `lld`, `libvulkan1`, `mesa-vulkan-drivers`, `python3`, dan `zenity`. Totalnya sekitar 150–250 MB.

Sebelum mulai, cek dulu apakah Vulkan sudah jalan:
```bash
sudo apt install vulkan-tools
vulkaninfo --summary | grep -i "deviceName\|apiVersion"
# harus muncul: Intel(R) HD Graphics 620 (KBL GT2) ... apiVersion 1.3.x
```

## 2. Taruh file game

Taruh copy milikmu sendiri di folder ini:
```
~/.local/share/black-linux/game/
    Black.iso                    ← file disc image (USA atau Europe), ATAU
    default.xbe + folder-foldernya (hasil ekstrak)
```
Kalau folder itu kosong, game akan membuka dialog untuk memilih file `.iso`-nya.

## 3. Jalankan pertama kali

Buka menu, lalu **Games → Black (Linux Native)**. Atau lewat terminal (supaya progresnya kelihatan):
```bash
black-linux-build      # build saja, ada progres
black-linux            # main
```

Start pertama akan **mengompilasi game khusus untuk PC-mu**:
1. `default.xbe` direcompile ke C++ (64 bagian).
2. Hasilnya dikompilasi dengan clang. **Di X270 perkiraannya 30–90 menit, dan cuma sekali.**
3. Semuanya di-link jadi `~/.local/share/black-linux/bin/BLACK`.

Setelah itu, setiap kali main game langsung jalan.

## 4. Kontrol dan pengaturan

| Tombol / perintah | Fungsi |
|---|---|
| `F11` atau `Alt+Enter` | Ganti antara fullscreen dan jendela |
| Menu Options di dalam game | Graphics, Controller (deadzone/sensitivitas stik), General, Mouse and Keyboard (bisa rebind), Cheats |
| `F10` | Panel setting overlay |
| `BLACK_TOUCH=1 black-linux` | Paksa tampilkan tombol sentuh (hanya untuk laptop layar sentuh) |
| `BLACK_GPU=nvidia black-linux` | Pilih GPU tertentu berdasarkan namanya |
| `black-linux --windowed` | Mulai dalam mode jendela |
| `black-linux --rebuild` | Build ulang |

**Setting yang disarankan untuk Intel HD 620:**
- Render Resolution: 100% di layar 1366×768, atau sekitar 67% di layar 1080p. Nilai ini sudah otomatis diatur saat start pertama.
- MSAA/SMAA: off.
- Ambient Occlusion: off.
- Draw Distance: 1x.
- V-Sync: on (default).

## 5. Lokasi file

| Path | Isi |
|---|---|
| `~/.local/share/black-linux/black.toml` | Pengaturan |
| `~/.local/share/black-linux/hdd/` | Save game |
| `~/.local/share/black-linux/logs/` | Log, dibutuhkan kalau ada error |
| `~/.local/share/black-linux/bin/BLACK` | Game yang sudah di-build |
| `~/.cache/black-linux/build.log` | Log proses build |

## 6. Troubleshooting

| Masalah | Solusi |
|---|---|
| `compiling failed` | Kemungkinan RAM habis. Jalankan `black-linux-build --jobs 1` (lebih lambat tapi hemat RAM). |
| `the recompiler stopped` | Bukan rilis USA/Europe, atau file ISO rusak. Cek `build.log`. |
| Layar hitam atau crash saat start | `sudo apt install --reinstall mesa-vulkan-drivers`, lalu kirim isi `logs/`. |
| Muncul tombol sentuh Android di layar | Sudah diperbaiki di 0.1.2. Update `.deb`, lalu jalankan `black-linux-build` (cuma link ulang, sekitar 1 menit). |
| Muncul peringatan "different release" | Copy game-nya beda versi dan kemungkinan besar crash. |

---

## Cara kerja (untuk yang penasaran)

```
 PAKET .deb (tanpa kode game)                     DI LAPTOPMU (sekali)
 ┌──────────────────────────────┐   default.xbe  ┌────────────────────────────────┐
 │ xbe_recomp  (recompiler)     │ ─────────────▶ │ 64 file C++ hasil recompile    │
 │ libblack_host.a:             │                │        │ clang -O2              │
 │  - runtime XboxXBE-Recomp    │                │        ▼                        │
 │    (kernel Xbox, D3D8, DSnd) │   link         │ objek game  +  host  =  BLACK   │
 │  - hooks/sound/D3D8 game     │ ─────────────▶ │ (ELF x86_64 native, Vulkan)     │
 │  - host desktop SDL3+Vulkan  │                └────────────────────────────────┘
 │ libSDL3.a, libglslang.a      │
 └──────────────────────────────┘
```

Proyek ini dibangun di atas:
- **XboxXBE-Recomp SDK** oleh TGP482 (MIT): recompiler dan runtime portable. Dipin di commit `99078b1`.
- **Black for Android** oleh RanggaCasper (MIT): `src/` (hooks, sound, D3D8) dan renderer Vulkan. Folder `desktop/` adalah host Android yang diadaptasi ke desktop Linux.

### Apa yang diubah untuk desktop Linux
- `desktop/main.cpp`: window desktop (fullscreen/jendela, F11), folder data XDG, info sistem dari `/proc`, default yang cocok untuk laptop.
- `desktop/game_files.cpp`: mencari game di `~/.local/share/black-linux/game`, `~/Games/Black`, atau `$BLACK_GAME_DIR`. Semua bagian JNI/Android dihapus.
- `desktop/input.cpp`, `update.cpp`, `touch.cpp`: bagian JNI dihapus, dan path skin diarahkan ke `/usr/share`.
- `desktop/vulkan.cpp`: surface Vulkan tidak lagi butuh window Android (sumber crash `renderer init failed` di 0.1.0), dan GPU dipilih otomatis: diskrit, lalu terintegrasi.
- `desktop/touch.cpp`: tombol sentuh disembunyikan dan mode awal pakai keyboard/mouse. Overlay baru muncul kalau layar sentuh benar-benar disentuh.
- `src/common/menu.cpp`: halaman Touch/Gyro diganti halaman Controller, dan menu "Check for updates" (khusus Android) dihapus.
- `src/d3d/render_thread.cpp`: perbandingan state yang tadinya pakai NEON (ARM) diberi versi **SSE2** untuk x86_64.
- `sdk/tools/recomp/main.cpp`: mode `XBE_HEADER_ONLY`, supaya host bisa di-build tanpa file game.

### Build .deb sendiri dari source (tanpa file game)
```bash
./build-from-source.sh
sudo apt install ./release/black-linux_*.deb
```

## Status pengujian (versi 0.1.2)

| Bagian | Status |
|---|---|
| Recompile dengan ISO asli (USA, sha1 `82d05881…`) di ThinkPad X270 | ✅ |
| Renderer Vulkan di Intel HD 620 | ✅ lancar (sejak 0.1.1) |
| Tombol sentuh Android tidak muncul di laptop | ✅ 0.1.2 |
| Update host tanpa compile ulang penuh (cache objek) | ✅ sejak 0.1.1 |

## Kredit

- **XboxXBE-Recomp SDK** oleh TGP482 (MIT): `sdk/`.
- **Black for Android** oleh Rangga Casper (MIT): `src/`, `desktop/`, `config/`, `data/`.
- Library pihak ketiga (SDL3, Dear ImGui, glslang, Zydis, toml++) diunduh saat build. Lihat `sdk/THIRD_PARTY.txt`.
- Port desktop Linux dan paket `.deb`: **Katon17**.

Lisensi: MIT (lihat `LICENSE`).
