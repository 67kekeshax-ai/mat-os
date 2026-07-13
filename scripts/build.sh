#!/bin/bash
# MAT OS — Скрипт сборки
# Запускать от root на Debian/Ubuntu хосте.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

log() { echo -e "\e[32m[MAT OS BUILD]\e[0m $*"; }
err() { echo -e "\e[31m[ERROR]\e[0m $*" >&2; exit 1; }

# Проверка прав root
if [[ "$EUID" -ne 0 ]]; then
    err "Запускайте от root: sudo ./scripts/build.sh"
fi

log "Установка зависимостей сборки..."
apt-get update -q
apt-get install -y \
    build-essential g++ make \
    libx11-dev libxext-dev \
    debootstrap squashfs-tools \
    grub-pc-bin grub-efi-amd64-bin \
    xorriso mtools \
    parted dosfstools e2fsprogs \
    libcrypt-dev \
    2>/dev/null || true

# Создать папку output
mkdir -p "$PROJECT_DIR/output"

# ─── Сборка установщика ───────────────────────────────────────────────────
log "Сборка установщика..."
cd "$PROJECT_DIR/installer"
make -j"$(nproc)" || err "Ошибка сборки установщика"
install -Dm755 matos-installer /usr/local/bin/matos-installer
log "matos-installer → /usr/local/bin/"

# ─── Сборка оболочки ──────────────────────────────────────────────────────
log "Сборка оболочки рабочего стола..."
cd "$PROJECT_DIR/shell"
make -j"$(nproc)" || err "Ошибка сборки оболочки"
install -Dm755 matos-shell /usr/local/bin/matos-shell
install -Dm755 matos-login /usr/local/bin/matos-login
log "matos-shell, matos-login → /usr/local/bin/"

# Assets
mkdir -p /opt/matos-shell/assets
if command -v convert &>/dev/null; then
    log "Генерация ресурсов через ImageMagick..."
    convert -size 1920x1080 gradient:"#032D60-#0078D7" \
            /opt/matos-shell/assets/wallpaper.png 2>/dev/null || true
    convert -size 48x48 xc:none \
        -fill '#F65314' -draw 'rectangle 2,2 22,22' \
        -fill '#7CBB00' -draw 'rectangle 26,2 46,22' \
        -fill '#00A1F1' -draw 'rectangle 2,26 22,46' \
        -fill '#FFBB00' -draw 'rectangle 26,26 46,46' \
        /opt/matos-shell/assets/start_button.png 2>/dev/null || true
fi

# Скрипт настроек
install -Dm755 "$PROJECT_DIR/config/includes.chroot/opt/matos-shell/matos-settings.sh" \
               /opt/matos-shell/matos-settings.sh

# ─── Создание ISO ─────────────────────────────────────────────────────────
log "Создание загрузочного ISO..."
cd "$PROJECT_DIR"
chmod +x scripts/make-iso.sh
bash scripts/make-iso.sh

log ""
log "════════════════════════════════════════════════════"
log "  MAT OS ISO готов: output/matos.iso"
log "════════════════════════════════════════════════════"
log ""
log "Тест в QEMU:"
log "  qemu-system-x86_64 -enable-kvm -cdrom output/matos.iso -m 2G -vga std -boot d"
