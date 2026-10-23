## black-linux 0.1.2

Port **native Linux** untuk *Black* (Xbox, 2006): static recompilation → ELF x86_64, Vulkan + SDL3. Bukan emulator, bukan Wine.

> ⚠️ Tidak berisi kode/aset game. Pakai copy game milikmu sendiri (USA/Europe, `.iso` atau folder berisi `default.xbe`).

**Diuji:** ThinkPad X270 (Intel HD 620, 8 GB RAM, Linux Mint 22.3) dengan grafik rata kanan: **90–100+ FPS**.

### Install (Ubuntu 24.04 / Linux Mint 22.x)
```bash
sudo apt install ./black-linux_0.1.2_amd64.deb
mkdir -p ~/.local/share/black-linux/game
cp Black.iso ~/.local/share/black-linux/game/
black-linux-build    # sekali, 30–90 menit
black-linux
```

### Perubahan
- 0.1.2: tombol sentuh Android disembunyikan di desktop, mode awal pakai keyboard/mouse, halaman Touch diganti Controller, menu "Check for updates" dihapus.
- 0.1.1: fix crash `renderer init failed` (surface Vulkan tidak lagi butuh window Android), GPU dipilih otomatis (`BLACK_GPU=` untuk override), cache objek: update host cukup link ulang.
