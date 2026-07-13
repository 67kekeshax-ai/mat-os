#!/bin/bash
# MAT OS — Создание загрузочного ISO
# Вызывается из build.sh. Требует root.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/.build"
OUTPUT="$PROJECT_DIR/output/matos.iso"

log() { echo -e "\e[36m[ISO]\e[0m $*"; }

CHROOT="$BUILD_DIR/chroot"
ISO_DIR="$BUILD_DIR/iso"

# ─── Базовая система (debootstrap) ───────────────────────────────────────
if [[ ! -d "$CHROOT/usr" ]]; then
    log "debootstrap Debian Bookworm → $CHROOT ..."
    mkdir -p "$CHROOT"
    debootstrap --arch=amd64 bookworm "$CHROOT" http://deb.debian.org/debian
else
    log "Используем существующий chroot: $CHROOT"
fi

# ─── Установка пакетов в chroot ──────────────────────────────────────────
log "Установка пакетов..."
cat > "$CHROOT/tmp/pkgs.sh" << 'EOF'
#!/bin/bash
export DEBIAN_FRONTEND=noninteractive
apt-get update -q
apt-get install -y --no-install-recommends \
    linux-image-amd64 \
    live-boot live-boot-initramfs-tools \
    systemd systemd-sysv \
    xorg xinit x11-xserver-utils xterm \
    libx11-6 libxext6 \
    network-manager \
    fonts-dejavu-core \
    sudo passwd \
    bash coreutils util-linux \
    parted e2fsprogs dosfstools \
    debootstrap \
    locales \
    xserver-xorg-input-vmmouse \
    grub-pc 
apt-get clean
EOF
chmod +x "$CHROOT/tmp/pkgs.sh"
chroot "$CHROOT" /tmp/pkgs.sh

# ─── Копирование компонентов MAT OS ─────────────────────────────────────
log "Копирование MAT OS бинарей..."
cp /usr/local/bin/matos-installer "$CHROOT/usr/local/bin/" 2>/dev/null || true
cp /usr/local/bin/matos-shell     "$CHROOT/usr/local/bin/" 2>/dev/null || true
cp /usr/local/bin/matos-login     "$CHROOT/usr/local/bin/" 2>/dev/null || true
mkdir -p "$CHROOT/opt/matos-shell/assets"
cp -r /opt/matos-shell/assets/. "$CHROOT/opt/matos-shell/assets/" 2>/dev/null || true

# Скрипт настроек
install -Dm755 \
    "$PROJECT_DIR/config/includes.chroot/opt/matos-shell/matos-settings.sh" \
    "$CHROOT/opt/matos-shell/matos-settings.sh"

# ─── Конфигурация системы ────────────────────────────────────────────────
log "Конфигурация системы..."

# Hostname
echo "matos-live" > "$CHROOT/etc/hostname"
cat > "$CHROOT/etc/hosts" << 'HOSTS'
127.0.0.1   localhost
127.0.1.1   matos-live
HOSTS

# Locale
echo "ru_RU.UTF-8 UTF-8" >> "$CHROOT/etc/locale.gen"
echo "en_US.UTF-8 UTF-8" >> "$CHROOT/etc/locale.gen"
chroot "$CHROOT" locale-gen
echo "LANG=ru_RU.UTF-8" > "$CHROOT/etc/locale.conf"

# Timezone
chroot "$CHROOT" ln -sf /usr/share/zoneinfo/Europe/Moscow /etc/localtime

# Live пользователь (user/live для live-режима)
chroot "$CHROOT" useradd -m -s /bin/bash user 2>/dev/null || true
echo "user:live" | chroot "$CHROOT" chpasswd
chroot "$CHROOT" usermod -aG sudo user

# matos_login service user
chroot "$CHROOT" useradd -r -s /usr/local/bin/matos-login matos_login 2>/dev/null || true

# .xinitrc
cat > "$CHROOT/home/user/.xinitrc" << 'XINITRC'
#!/bin/sh
exec /usr/local/bin/matos-shell
XINITRC
chroot "$CHROOT" chown user:user /home/user/.xinitrc
chmod +x "$CHROOT/home/user/.xinitrc"

# Автологин через getty
mkdir -p "$CHROOT/etc/systemd/system/getty@tty1.service.d"
cat > "$CHROOT/etc/systemd/system/getty@tty1.service.d/autologin.conf" << 'AUTOLOGIN'
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin matos_login --noclear %I $TERM
AUTOLOGIN

# NetworkManager
chroot "$CHROOT" systemctl enable NetworkManager 2>/dev/null || true

# Edition (live = HOME)
echo "EDITION=MAT OS HOME (Live)" > "$CHROOT/etc/matos-edition"
echo "KEY=12006769"               >> "$CHROOT/etc/matos-edition"

# Сеть
cp "$PROJECT_DIR/config/includes.chroot/etc/network/interfaces" \
   "$CHROOT/etc/network/interfaces"
cp "$PROJECT_DIR/config/includes.chroot/etc/NetworkManager/NetworkManager.conf" \
   "$CHROOT/etc/NetworkManager/NetworkManager.conf"

# ─── SquashFS ────────────────────────────────────────────────────────────
log "Создание SquashFS образа..."
mkdir -p "$ISO_DIR/live"
mksquashfs "$CHROOT" "$ISO_DIR/live/filesystem.squashfs" \
    -comp xz -e boot 2>&1 | tail -5

# ─── Ядро и initrd ───────────────────────────────────────────────────────
log "Копирование ядра и initrd..."
mkdir -p "$ISO_DIR/boot"
KERNEL=$(ls "$CHROOT/boot/vmlinuz-"* 2>/dev/null | sort -V | tail -1)
INITRD=$(ls "$CHROOT/boot/initrd.img-"* 2>/dev/null | sort -V | tail -1)

if [[ -z "$KERNEL" ]]; then
    log "ПРЕДУПРЕЖДЕНИЕ: ядро не найдено в chroot"
else
    cp "$KERNEL" "$ISO_DIR/boot/vmlinuz"
    cp "$INITRD"  "$ISO_DIR/boot/initrd.img"
fi

# ─── GRUB ────────────────────────────────────────────────────────────────
log "Настройка GRUB..."
mkdir -p "$ISO_DIR/boot/grub"
cp "$PROJECT_DIR/bootloader/grub.cfg" "$ISO_DIR/boot/grub/grub.cfg"
mkdir -p "$ISO_DIR/boot/grub/themes/matos"
cp "$PROJECT_DIR/bootloader/theme/theme.txt" \
   "$ISO_DIR/boot/grub/themes/matos/theme.txt"

# Исправляем grub.cfg для live-режима
cat > "$ISO_DIR/boot/grub/grub.cfg" << 'GRUBCFG'
set default=0
set timeout=5

menuentry "MAT OS (Live)" {
    linux /boot/vmlinuz boot=live quiet splash
    initrd /boot/initrd.img
}
menuentry "MAT OS (Live, безопасный режим)" {
    linux /boot/vmlinuz boot=live nomodeset xforcevesa
    initrd /boot/initrd.img
}
menuentry "Установить MAT OS на диск" {
    linux /boot/vmlinuz boot=live quiet splash
    initrd /boot/initrd.img
}
GRUBCFG

# ─── Создание ISO ────────────────────────────────────────────────────────
log "Сборка ISO через grub-mkrescue..."
mkdir -p "$PROJECT_DIR/output"
grub-mkrescue -o "$OUTPUT" "$ISO_DIR" 2>&1 | tail -10

log "ISO создан: $OUTPUT ($(du -sh "$OUTPUT" | cut -f1))"
