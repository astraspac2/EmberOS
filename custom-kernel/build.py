#!/usr/bin/env python3
"""Build EmberOS: compile the kernel with clang, then pack a bootable ISO.

Needs only LLVM (clang + ld.lld) and Python. The ISO9660/El Torito writer is
built in, so no xorriso is required. Boots on BIOS and UEFI (VMware, QEMU).
"""
import os
import shutil
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(ROOT, "build")
LIMINE = os.path.join(ROOT, "third_party", "limine", "limine-binary")
LIMINE_INC = os.path.join(ROOT, "third_party", "limine")
SECTOR = 2048


def find_tool(name):
    path = shutil.which(name)
    if path:
        return path
    candidate = os.path.join(os.environ.get("ProgramFiles", r"C:\Program Files"), "LLVM", "bin", name + ".exe")
    if os.path.exists(candidate):
        return candidate
    sys.exit(f"error: {name} not found (install LLVM)")


def build_kernel():
    clang = find_tool("clang")
    lld = find_tool("ld.lld")
    os.makedirs(BUILD, exist_ok=True)
    elf = os.path.join(BUILD, "kernel.elf")
    src_dir = os.path.join(ROOT, "kernel")

    objs = []
    for name in sorted(os.listdir(src_dir)):
        if not name.endswith((".c", ".S")):
            continue
        obj = os.path.join(BUILD, name + ".o")
        objs.append(obj)
        subprocess.check_call([
            clang, "--target=x86_64-unknown-none-elf", "-std=c11", "-O2", "-g",
            "-Wall", "-Wextra", "-ffreestanding", "-fno-stack-protector",
            "-fno-stack-check", "-fno-lto", "-fno-PIC", "-fno-pie",
            "-mno-red-zone", "-mcmodel=kernel", "-mno-mmx", "-mno-sse", "-mno-sse2",
            "-I", src_dir, "-I", LIMINE_INC,
            "-c", os.path.join(src_dir, name), "-o", obj,
        ])
    subprocess.check_call([
        lld, "-nostdlib", "-static", "-z", "max-page-size=0x1000",
        "-T", os.path.join(src_dir, "linker.ld"), *objs, "-o", elf,
    ])
    return elf


# ---------------------------------------------------------------- ISO9660 ---

def both32(n):
    return struct.pack("<I", n) + struct.pack(">I", n)


def both16(n):
    return struct.pack("<H", n) + struct.pack(">H", n)


def pad(data, size, fill=b" "):
    return data[:size].ljust(size, fill)


DATE = bytes([126, 1, 1, 0, 0, 0, 0])  # 2026-01-01 00:00:00 UTC


def dir_record(lba, size, is_dir, ident):
    ident_len = len(ident)
    length = 33 + ident_len
    if length % 2:
        length += 1
    rec = struct.pack("<BB", length, 0) + both32(lba) + both32(size) + DATE
    rec += struct.pack("<BBB", 2 if is_dir else 0, 0, 0) + both16(1)
    rec += struct.pack("<B", ident_len) + ident
    return rec.ljust(length, b"\0")


class Dir:
    def __init__(self, name, parent):
        self.name = name
        self.parent = parent
        self.dirs = []
        self.files = []  # (name, bytes)
        self.lba = 0
        self.index = 0
        if parent:
            parent.dirs.append(self)

    def add_file(self, name, data):
        self.files.append((name, data))


def build_iso(out_path, kernel_elf):
    def read(path):
        with open(path, "rb") as f:
            return f.read()

    root = Dir("", None)
    boot = Dir("boot", root)
    boot_limine = Dir("limine", boot)
    efi = Dir("EFI", root)
    efi_boot = Dir("BOOT", efi)

    bios_cd = bytearray(read(os.path.join(LIMINE, "limine-bios-cd.bin")))
    uefi_cd = read(os.path.join(LIMINE, "limine-uefi-cd.bin"))

    boot.add_file("kernel.elf", read(kernel_elf))
    boot_limine.add_file("limine.conf", read(os.path.join(ROOT, "limine.conf")))
    boot_limine.add_file("limine-bios.sys", read(os.path.join(LIMINE, "limine-bios.sys")))
    boot_limine.add_file("limine-bios-cd.bin", bytes(bios_cd))
    boot_limine.add_file("limine-uefi-cd.bin", uefi_cd)
    efi_boot.add_file("BOOTX64.EFI", read(os.path.join(LIMINE, "BOOTX64.EFI")))

    # Directory order for the path table: breadth first.
    order = [root]
    i = 0
    while i < len(order):
        d = order[i]
        d.dirs.sort(key=lambda x: x.name.encode())
        order.extend(d.dirs)
        i += 1
    for n, d in enumerate(order, start=1):
        d.index = n

    # Fixed layout: 16 system, PVD, boot record, terminator, catalog, 2 path tables.
    lba_pvd, lba_boot, lba_term, lba_cat, lba_ptl, lba_ptm = 16, 17, 18, 19, 20, 21
    next_lba = 22
    for d in order:
        d.lba = next_lba
        next_lba += 1  # every directory here fits in one sector

    file_lba = {}
    for d in order:
        for name, data in d.files:
            file_lba[(id(d), name)] = next_lba
            next_lba += max(1, -(-len(data) // SECTOR))
    total_sectors = next_lba

    def dir_ident(d):
        return d.name.encode()

    # Patch the El Torito boot info table inside limine-bios-cd.bin.
    bios_lba = file_lba[(id(boot_limine), "limine-bios-cd.bin")]
    checksum = 0
    for off in range(64, len(bios_cd) - len(bios_cd) % 4, 4):
        checksum = (checksum + struct.unpack_from("<I", bios_cd, off)[0]) & 0xFFFFFFFF
    struct.pack_into("<IIII", bios_cd, 8, lba_pvd, bios_lba, len(bios_cd), checksum)
    for k, (name, _data) in enumerate(boot_limine.files):
        if name == "limine-bios-cd.bin":
            boot_limine.files[k] = (name, bytes(bios_cd))

    img = bytearray(total_sectors * SECTOR)

    def put(lba, data):
        img[lba * SECTOR: lba * SECTOR + len(data)] = data

    # Directories.
    for d in order:
        parent = d.parent or d
        entries = [dir_record(d.lba, SECTOR, True, b"\0"), dir_record(parent.lba, SECTOR, True, b"\1")]
        items = [(sd.name, sd, None) for sd in d.dirs] + [(n, None, data) for n, data in d.files]
        for name, sub, data in sorted(items, key=lambda t: t[0].encode()):
            if sub:
                entries.append(dir_record(sub.lba, SECTOR, True, name.encode()))
            else:
                entries.append(dir_record(file_lba[(id(d), name)], len(data), False, name.encode()))
        blob = b"".join(entries)
        assert len(blob) <= SECTOR, "directory too large"
        put(d.lba, blob)
        for name, data in d.files:
            put(file_lba[(id(d), name)], data)

    # Path tables.
    def path_table(endian):
        out = b""
        for d in order:
            ident = dir_ident(d) if d.parent else b"\0"
            parent_idx = d.parent.index if d.parent else 1
            rec = struct.pack("<BB", len(ident), 0)
            rec += struct.pack(endian + "IH", d.lba, parent_idx) + ident
            if len(ident) % 2:
                rec += b"\0"
            out += rec
        return out

    ptl, ptm = path_table("<"), path_table(">")
    put(lba_ptl, ptl)
    put(lba_ptm, ptm)

    # Primary volume descriptor.
    pvd = bytearray(SECTOR)
    pvd[0:8] = b"\x01CD001\x01\x00"
    pvd[8:40] = pad(b"EMBEROS", 32)
    pvd[40:72] = pad(b"EMBEROS", 32)
    pvd[80:88] = both32(total_sectors)
    pvd[120:124] = both16(1)
    pvd[124:128] = both16(1)
    pvd[128:132] = both16(SECTOR)
    pvd[132:140] = both32(len(ptl))
    pvd[140:144] = struct.pack("<I", lba_ptl)
    pvd[148:152] = struct.pack(">I", lba_ptm)
    pvd[156:190] = dir_record(root.lba, SECTOR, True, b"\0")
    for start, size in ((190, 128), (318, 128), (446, 128), (574, 128),
                        (702, 37), (739, 37), (776, 37)):
        pvd[start:start + size] = b" " * size
    for start in (813, 830, 847, 864):
        pvd[start:start + 17] = b"0" * 16 + b"\0"
    pvd[881] = 1
    put(lba_pvd, bytes(pvd))

    # El Torito boot record and terminator.
    br = bytearray(SECTOR)
    br[0:7] = b"\x00CD001\x01"
    br[7:39] = pad(b"EL TORITO SPECIFICATION", 32, b"\0")
    br[71:75] = struct.pack("<I", lba_cat)
    put(lba_boot, bytes(br))
    put(lba_term, b"\xffCD001\x01")

    # Boot catalog: BIOS default entry, then a UEFI section.
    validation = bytearray(32)
    validation[0] = 1
    validation[1] = 0  # x86 BIOS
    validation[30:32] = b"\x55\xaa"
    words = sum(struct.unpack("<16H", bytes(validation))) & 0xFFFF
    struct.pack_into("<H", validation, 28, (-words) & 0xFFFF)

    bios_entry = struct.pack("<BBHBBHI", 0x88, 0, 0, 0, 0, 4, bios_lba) + bytes(20)
    uefi_lba = file_lba[(id(boot_limine), "limine-uefi-cd.bin")]
    uefi_sectors = min(65535, -(-len(uefi_cd) // 512))
    section = struct.pack("<BBH", 0x91, 0xEF, 1) + bytes(28)
    uefi_entry = struct.pack("<BBHBBHI", 0x88, 0, 0, 0, 0, uefi_sectors, uefi_lba) + bytes(20)
    put(lba_cat, bytes(validation) + bios_entry + section + uefi_entry)

    with open(out_path, "wb") as f:
        f.write(img)
    print(f"wrote {out_path} ({len(img) / 1024 / 1024:.1f} MiB, {total_sectors} sectors)")


def main():
    kernel = build_kernel()
    build_iso(os.path.join(ROOT, "EmberOS.iso"), kernel)


if __name__ == "__main__":
    main()
