## black-linux 0.1.2

A **native Linux** port of *Black* (Xbox, 2006): static recompilation → x86_64 ELF, Vulkan + SDL3. Not an emulator or Wine.

> ⚠️ Contains no game code or assets. Bring your own copy of the game (USA/Europe, `.iso` or an extracted folder containing `default.xbe`).

**Reported test results:** ThinkPad X270 (i7-7600U, Intel HD 620, 8 GB single-channel RAM, Linux Mint 22.3 XFCE): stable **80–100 FPS** observed during the first three missions at 1280×720, 100% render scale, FXAA, and V-Sync off. Instantaneous screenshots show 98.1 FPS facing sunlit windows and 139.0 FPS facing a door in Mission 1; these are not run averages. See the [README benchmark](README.md#benchmark) for full settings and screenshots. Results may vary on other hardware.

### Install (Ubuntu 24.04 / Linux Mint 22.x)

```bash
sudo apt install ./black-linux_0.1.2_amd64.deb
mkdir -p ~/.local/share/black-linux/game
cp Black.iso ~/.local/share/black-linux/game/
black-linux-build    # One-time build, approximately 30–90 minutes
black-linux
```

### Changes

- 0.1.2: hide Android touch controls on desktop, default to keyboard/mouse input, replace the Touch page with Controller, and remove the "Check for updates" menu.
- 0.1.1: fix the `renderer init failed` crash (Vulkan surfaces no longer require an Android window), automatically select a GPU (`BLACK_GPU=` overrides this), and cache object files so host updates only require relinking.

### Documentation

See the [English README](README.md) or [Bahasa Indonesia README](README.id.md) for setup, controls, and troubleshooting.
