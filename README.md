# EmberOS

A hobby operating system written from scratch in C, with its own x86-64 kernel, a terminal shell, and a custom ember-themed desktop called **the Hearth**.

> **Status:** early development. Nothing boots yet. See the roadmap below.

## Goals

- Write a real kernel: memory management, interrupts, a scheduler, and drivers.
- Build a distinctive desktop with no traditional taskbar: a glowing ember launcher, minimal windows, and a thin status strip.
- Make something usable day to day: a terminal, a text editor, a file browser, and a filesystem that survives reboots.

## Roadmap

- [ ] 1. **Boot:** kernel starts and prints "Hello from EmberOS"
- [ ] 2. **Core:** memory management, interrupts, timer, keyboard input
- [ ] 3. **Terminal:** text shell with `help`, `clear`, `echo`, `mem`, `reboot`
- [ ] 4. **Graphics:** framebuffer, font rendering, mouse cursor
- [ ] 5. **The Hearth:** windows, ember launcher, status strip
- [ ] 6. **Apps:** terminal window, text editor, clock, file browser
- [ ] 7. **Persistence:** simple filesystem

## Not planned (for now)

Networking, sound, USB, real hardware support, and running Windows or Linux programs.

## Building

Requirements (Windows):

- [LLVM/clang](https://llvm.org/) (with `lld`)
- Python 3

Build instructions will be added once the first kernel boots.

## Running

EmberOS targets 64-bit x86 virtual machines. The build produces a bootable `EmberOS.iso` that can be attached to a **VMware Workstation** VM (guest OS: "Other 64-bit", no hard disk needed yet).

## License

Not chosen yet. Until a license is added, all rights are reserved.
