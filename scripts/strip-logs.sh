#!/bin/bash
# MAT OS — Очистка логов перед публикацией ISO
# Удаляет: /var/log/*, apt-кэш, bash-историю, SSH-ключи, machine-id, man-страницы.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
CHROOT="$PROJECT_DIR/.build/chroot"

log() { echo -e "\e[35m[STRIP]\e[0m $*"; }
err() { echo -e "\e[31m[ERROR]\e[0m $*" >&2; exit 1; }

if [[ "$EUID" -ne 0 ]]; then
    err "Запускайте от root: sudo ./scripts/strip-logs.sh"
fi

if [[ ! -d "$CHROOT" ]]; then
    err "Chroot не найден: $CHROOT. Сначала запустите build.sh."
fi

log "Очистка /var/log/..."
find "$CHROOT/var/log" -type f -exec truncate -s 0 {} \; 2>/dev/null || true
rm -rf "$CHROOT/var/log/apt" 2>/dev/null || true

log "Очистка apt-кэша..."
rm -rf "$CHROOT/var/cache/apt/archives/"*.deb 2>/dev/null || true
rm -rf "$CHROOT/var/cache/apt/archives/partial/"* 2>/dev/null || true

log "Очистка bash-истории..."
find "$CHROOT/root" "$CHROOT/home" -name ".bash_history" \
     -exec truncate -s 0 {} \; 2>/dev/null || true

log "Удаление SSH host-ключей..."
rm -f "$CHROOT/etc/ssh/ssh_host_"* 2>/dev/null || true

log "Сброс machine-id..."
echo "" > "$CHROOT/etc/machine-id"
rm -f "$CHROOT/var/lib/dbus/machine-id" 2>/dev/null || true

log "Очистка temp файлов..."
rm -rf "$CHROOT/tmp/"* "$CHROOT/var/tmp/"* 2>/dev/null || true

log "Удаление man-страниц и документации..."
rm -rf "$CHROOT/usr/share/doc/"* 2>/dev/null || true
rm -rf "$CHROOT/usr/share/man/"* 2>/dev/null || true
rm -rf "$CHROOT/usr/share/info/"* 2>/dev/null || true

log "Очистка dpkg списков..."
rm -rf "$CHROOT/var/lib/apt/lists/"* 2>/dev/null || true

log "Удаление сборочных скриптов из chroot..."
rm -f "$CHROOT/tmp/"*.sh 2>/dev/null || true

log "Очистка завершена."

# Пересобрать ISO после очистки
if [[ "${1:-}" == "--iso" ]]; then
    log "Пересборка ISO..."
    bash "$SCRIPT_DIR/make-iso.sh"
    log "ISO пересобран."
fi
