#!/bin/bash
# MAT OS — Очистка артефактов сборки
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

log() { echo -e "\e[33m[CLEAN]\e[0m $*"; }

log "Удаление объектных файлов установщика..."
cd "$PROJECT_DIR/installer" && make clean 2>/dev/null || true

log "Удаление объектных файлов оболочки..."
cd "$PROJECT_DIR/shell" && make clean 2>/dev/null || true

if [[ "${1:-}" == "--all" ]]; then
    log "Удаление chroot и ISO..."
    rm -rf "$PROJECT_DIR/.build"
    rm -f  "$PROJECT_DIR/output/matos.iso"
    log "Полная очистка завершена."
else
    log "Сборочные файлы удалены. Для удаления .build/ и ISO: ./clean.sh --all"
fi
