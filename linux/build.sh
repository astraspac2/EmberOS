#!/bin/bash
# Build the EmberOS (Debian + Budgie) live/installer ISO.
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
    --chroot-squashfs-compression-type xz \
    --bootloaders "syslinux,grub-efi" \
    --debian-installer none \
    --memtest none \
    --image-name emberos \
    --iso-application "EmberOS" \
    --iso-publisher "EmberOS project" \
    --iso-volume "EmberOS" \
    --bootappend-live "boot=live components quiet splash username=ember hostname=emberos locales=en_US.UTF-8 keyboard-layouts=de"

# ---- boot menu theme (best effort: a miss here must never fail the build) ----
theme_bootloaders() {
    local src=/usr/share/live/build/bootloaders
    if [ ! -d "$src" ]; then
        echo "theme: no bootloader templates found at $src"
        return 0
    fi

    mkdir -p config/bootloaders
    cp -r "$src"/. config/bootloaders/

    echo "theme: bootloader template files:"
    find config/bootloaders -type f | sort

    # Wording
    grep -rIl 'Debian' config/bootloaders 2>/dev/null | while read -r f; do
        sed -i 's/Debian GNU\/Linux/EmberOS/g; s/Debian/EmberOS/g' "$f"
    done

    # BIOS (syslinux) menu: background image and amber colours
    find config/bootloaders -name splash.svg | while read -r f; do
        cp boot/splash-640x480.png "$(dirname "$f")/splash.png"
        rm -f "$f"
    done
    find config/bootloaders -name splash.png | while read -r f; do
        cp boot/splash-640x480.png "$f"
    done
    find config/bootloaders -name stdmenu.cfg | while read -r f; do
        cat >> "$f" <<'EOF'

menu color screen   37;40 #80ffffff #00000000 std
menu color border   30;44 #ffff9628 #00000000 std
menu color title    1;36;44 #ffff9628 #00000000 std
menu color unsel    37;44 #fff0e4da #00000000 std
menu color sel      7;37;40 #ff1a0e06 #ffff9628 all
menu color hotsel   1;7;37;40 #ff1a0e06 #ffff9628 all
menu color hotkey   1;37;44 #ffff9628 #00000000 std
menu color tabmsg   31;40 #ffb59680 #00000000 std
menu color help     37;40 #fff0e4da #00000000 std
EOF
    done

    # UEFI (GRUB) menu: background image, and show the theme file in the build
    # log so its colours can be matched next time
    find config/bootloaders -path '*theme*' -name 'background.png' | while read -r f; do
        cp boot/grub-background.png "$f"
    done
    find config/bootloaders -name theme.txt | while read -r f; do
        cp boot/grub-theme.txt "$f"
        cp boot/grub-background.png "$(dirname "$f")/background.png"
        echo "theme: replaced $f"
    done
}

theme_bootloaders || echo "theme: skipped after an error (continuing)"

lb build 2>&1 | tee build.log

ls -lh ./*.iso
