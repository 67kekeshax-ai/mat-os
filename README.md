# MAT OS

Операционная система на базе Debian, визуально напоминающая Windows 10.

## Редакции и ключи активации

| Редакция     | Ключ активации |
|--------------|----------------|
| MAT OS PRO   | `67691200`     |
| MAT OS HOME  | `12006769`     |

## Структура проекта

```
mat-os/
├── installer/          # Установщик (C++, framebuffer)
│   ├── src/
│   │   ├── gui/        # Framebuffer рендерер + виджеты
│   │   ├── license/    # Проверка ключей активации
│   │   ├── disk/       # Разметка и форматирование диска
│   │   ├── install/    # Установка системы (debootstrap)
│   │   └── main.cpp    # Точка входа, state machine
│   └── Makefile
├── shell/              # Оболочка рабочего стола (C++, X11)
│   ├── src/
│   │   ├── wm/         # Оконный менеджер (Xlib)
│   │   ├── taskbar/    # Панель задач
│   │   ├── startmenu/  # Меню «Пуск»
│   │   ├── filemanager/# Файловый менеджер
│   │   ├── login/      # Экран входа (framebuffer)
│   │   ├── bsod/       # Синий экран смерти
│   │   └── main.cpp    # Точка входа оболочки
│   └── Makefile
├── bootloader/         # GRUB конфигурация и тема
├── scripts/            # Скрипты сборки и создания ISO
├── config/             # live-build конфигурация
├── assets/             # Обои, иконки, изображения
└── docs/               # Документация
```

## Быстрый старт (сборка ISO)

**Требования**: Debian/Ubuntu хост, root-права, интернет.

```bash
cd mat-os
chmod +x scripts/*.sh
sudo ./scripts/build.sh
```

ISO будет создан в `output/matos.iso`.

## Тестирование в QEMU

```bash
qemu-system-x86_64 \
  -enable-kvm \
  -cdrom output/matos.iso \
  -m 2048 \
  -vga std \
  -boot d
```

Подробнее: `docs/QEMU_TEST.md`

## Архитектура

Подробнее: `docs/ARCHITECTURE.md`

## Перед публикацией ISO

```bash
sudo ./scripts/strip-logs.sh --iso
```

Удаляет логи, историю bash, SSH-ключи хоста, machine-id.
