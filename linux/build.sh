#!/bin/bash
# Build the EmberOS (Debian + XFCE) live/installer ISO.
# Run as root on Debian/Ubuntu with live-build installed. CI runs this inside
# a Debian trixie container (see .github/workflows/build-linux-iso.yml).
set -euo pipefail
cd "$(dirname "$0")"

lb clean --purge 2>/dev/null || true

lb config \
    --distribution trixie \
    --architectures amd64 \
    --linux-flavours amd64 \
    --archive-areas "main contrib non-free-firmware" \
    --binary-images iso-hybrid \
    --bootloaders "syslinux,grub-efi" \
    --debian-installer none \
    --memtest none \
    --image-name emberos \
    --iso-application "EmberOS" \
    --iso-publisher "EmberOS project" \
    --iso-volume "EmberOS" \
    --bootappend-live "boot=live components quiet splash username=ember hostname=emberos locales=en_US.UTF-8 keyboard-layouts=de"

lb build 2>&1 | tee build.log

ls -lh ./*.iso
