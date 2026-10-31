# EmberOS

A warm, dark, amber-themed operating system with two tracks: a **Linux distribution** you can install and use every day, and a **hobby kernel** written from scratch in C.

> **Status:** EmberOS Linux 0.1 is out. The custom kernel track is in early development.

## Two tracks

1. **Linux track** (`linux/`): EmberOS as a Debian 13 based distribution with the **Budgie** desktop, a warm amber theme, and the **Calamares** graphical installer.
2. **Custom kernel track** (repo root, `kernel/`): a hobby OS with its own x86-64 kernel, a terminal shell and a desktop called **the Hearth**. Details further down.

## EmberOS Linux

### What you get

- Debian 13 (trixie) with the Linux kernel and Budgie desktop, dark with amber accents everywhere: panel, terminal, installer, boot menu and boot splash
- Graphical installer (Calamares) with an EmberOS slideshow
- Welcome window on first login, and a wallpaper chooser with four warm wallpapers
- Software store (GNOME Software) with Flathub, plus Firefox, VLC, Files, Terminal, Text Editor and screenshots (Print Screen)
- Printing (CUPS) and Bluetooth (Blueman) support, and hardware firmware (Wi-Fi and more) included
- Safe by default: firewall (ufw) on and automatic security updates
- Default keyboard layout US (QWERTY); the installer asks for yours

### System requirements

| | Minimum | Recommended |
|---|---|---|
| Processor | 64-bit (x86-64), 2 cores | 4 cores or more |
| Memory | 2 GB RAM | 4 GB RAM or more |
| Disk | 15 GB | 30 GB or more (SSD) |
| Graphics | any with 1024x768 | any modern GPU |
| Firmware | UEFI or legacy BIOS (turn Secure Boot off) | UEFI |
| USB stick | 4 GB | 8 GB |

### Getting the ISO

Download `EmberOS.iso` from the [latest release](../../releases/latest).

The ISO is built in the cloud by GitHub Actions: open the **Actions** tab and run **Build EmberOS Linux ISO** to build your own.

### Installing

1. Write `EmberOS.iso` to a USB stick with [Rufus](https://rufus.ie) (choose *DD image mode* if asked), [balenaEtcher](https://etcher.balena.io), or on Linux `sudo dd if=EmberOS.iso of=/dev/sdX bs=4M status=progress`.
2. Boot the computer from the USB stick (usually F12, F10, F2 or Esc at power-on) and pick **EmberOS**.
3. Try the live session (user `ember`, password `live`) or click **Install EmberOS** and follow the installer.

It also runs in a virtual machine (VMware, VirtualBox, and similar).

### How it is built

Everything lives in `linux/`: `build.sh` (live-build), `config/package-lists` (packages), `config/includes.chroot` (files copied into the image: theme, dconf defaults, apps, scripts), `config/hooks` (build-time setup) and `boot/` plus `tools/` (generated logo, wallpapers and boot images; plain Python, no dependencies).

## Custom kernel track

### Goals

- Write a real kernel: memory management, interrupts, a scheduler, and drivers.
- Build a distinctive desktop with no traditional taskbar: a glowing ember launcher, minimal windows, and a thin status strip.
- Make something usable day to day: a terminal, a text editor, a file browser, and a filesystem that survives reboots.

### Roadmap

- [x] 1. **Boot:** kernel starts and prints "Hello from EmberOS"
- [x] 2. **Core:** memory management, interrupts, timer, keyboard input (needs testing in VMware)
- [x] 3. **Terminal:** text shell with `help`, `clear`, `echo`, `mem`, `uptime`, `reboot`, `halt`, command history (needs testing in VMware)
- [x] 4. **Graphics:** double-buffered framebuffer, font rendering, PS/2 mouse cursor (needs testing in VMware)
- [x] 5. **The Hearth:** draggable windows, glowing launcher orb, status strip (needs testing in VMware)
- [x] 6. **Apps:** terminal, text editor, file browser, clock, system monitor (needs testing in VMware)
- [ ] 7. **Persistence:** files currently live in RAM only; a disk filesystem is next

The boot menu offers **EmberOS Desktop** (default) and **Safe Mode (shell only)**.

### Not planned (for now)

Networking, sound, USB, real hardware support, and running Windows or Linux programs.

### Building

Requirements (Windows):

- [LLVM/clang](https://llvm.org/) (with `lld`)
- Python 3

Download the [Limine](https://github.com/limine-bootloader/limine) v12.9.5 binary release into `third_party/limine/limine-binary/` and `limine.h` from [limine-protocol](https://github.com/limine-bootloader/limine-protocol) into `third_party/limine/` (these are not committed). Then:

```
python build.py
```

This compiles the kernel and writes `EmberOS.iso` in the project root. No `xorriso` is needed; the ISO writer is built in.

### Running

EmberOS targets 64-bit x86 virtual machines. The build produces a bootable `EmberOS.iso` that can be attached to a **VMware Workstation** VM (guest OS: "Other 64-bit", no hard disk needed yet).

## License

[MIT](LICENSE). The ISO also contains Debian and other software, each under its own licence.
