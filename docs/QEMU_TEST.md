# MAT OS — Тестирование в виртуальных машинах

## QEMU (рекомендуется — бесплатно, без GUI)

### Установка QEMU
```bash
# Debian/Ubuntu
sudo apt-get install -y qemu-system-x86

# Arch
sudo pacman -S qemu-full

# Fedora
sudo dnf install qemu-kvm
```

### Запуск ISO в QEMU
```bash
# Базовый запуск (VGA Standard, 2 ГБ RAM)
qemu-system-x86_64 \
  -cdrom output/matos.iso \
  -m 2048 \
  -vga std \
  -boot d

# Расширенный запуск (с KVM-ускорением, лучше производительность)
qemu-system-x86_64 \
  -enable-kvm \
  -cdrom output/matos.iso \
  -m 2048 \
  -cpu host \
  -vga virtio \
  -boot d \
  -display gtk

# Тест установки на виртуальный диск (20 ГБ)
qemu-img create -f qcow2 matos_test.qcow2 20G
qemu-system-x86_64 \
  -enable-kvm \
  -cdrom output/matos.iso \
  -hda matos_test.qcow2 \
  -m 2048 \
  -cpu host \
  -vga std \
  -boot d
```

## VirtualBox

1. **Создать виртуальную машину**:
   - Тип: Linux → Debian (64-bit)
   - RAM: 2048 МБ или больше
   - Диск: 20+ ГБ (VDI, динамический)

2. **Подключить ISO**:
   - Настройки → Носители → Контроллер IDE → Оптический привод
   - Выбрать `output/matos.iso`

3. **Настройки дисплея**:
   - Видеопамять: 64–128 МБ
   - Ускорение: 3D (опционально)

4. **Загрузить**: запустить VM

## VMware Workstation / Player

1. Создать новую VM (Linux → Debian 10+)
2. Settings → CD/DVD → Use ISO image file → указать `output/matos.iso`
3. Запустить VM

## Параметры загрузки GRUB

При старте GRUB предложит варианты:

| Пункт                | Описание                                    |
|----------------------|---------------------------------------------|
| MAT OS PRO           | Загрузка в live-режиме (HOME редакция)      |
| MAT OS HOME          | Загрузка в live-режиме (HOME редакция)      |
| MAT OS (Safe Mode)   | `nomodeset xforcevesa` — проблемная графика |
| MAT OS (Recovery)    | Однопользовательский режим                  |

> **Примечание**: live-режим всегда загружается в редакции HOME.
> Редакция определяется только при установке (по лицензионному ключу).

## Порядок работы с установщиком

1. Загрузиться с ISO → live-режим (автологин)
2. На рабочем столе нажать «Пуск» → «Установить MAT OS»
3. Ввести ключ:
   - **PRO**: `67691200`
   - **HOME**: `12006769`
4. Выбрать диск (осторожно — данные будут удалены!)
5. Ввести имя пользователя и пароль
6. Дождаться установки (~10–30 минут зависит от скорости интернета)
7. Перезагрузиться

## Типичные проблемы

| Проблема                       | Решение                                        |
|--------------------------------|------------------------------------------------|
| Чёрный экран после загрузки    | Выбрать «Safe Mode» в GRUB                     |
| X не запускается               | Проверить `/var/log/Xorg.0.log` в chroot       |
| Нет сети                       | `systemctl start NetworkManager` в терминале   |
| BSOD при старте оболочки       | Запустить shell с флагом --no-login в терминале|
| Установщик не видит диск       | Убедитесь что VM имеет подключённый vHDD        |
