#include "install.h"
#include "../disk/disk.h"

#include <cstdlib>
#include <fstream>
#include <string>

static int sh(const std::string& cmd) {
    return system(cmd.c_str());
}

static void write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
}

bool run_install(const InstallConfig& cfg, ProgressCallback cb) {
    const std::string MP = "/mnt/matos_target";

    cb(2, "Разметка диска...");
    PartitionPlan plan { cfg.disk, cfg.uefi };
    if (!partition_disk(plan)) return false;

    cb(8, "Форматирование...");
    if (!format_disk(cfg.disk, cfg.uefi)) return false;

    cb(12, "Монтирование...");
    if (!mount_disk(cfg.disk, MP, cfg.uefi)) return false;

    cb(15, "Установка базовой системы (debootstrap)...");
    if (sh("debootstrap --arch=amd64 bookworm " + MP +
           " http://deb.debian.org/debian") != 0) return false;

    // fstab
    cb(35, "Настройка fstab...");
    sh("genfstab -U " + MP + " >> " + MP + "/etc/fstab");

    // hostname
    cb(37, "Настройка имени компьютера...");
    write_file(MP + "/etc/hostname", cfg.hostname + "\n");
    write_file(MP + "/etc/hosts",
        "127.0.0.1 localhost\n127.0.1.1 " + cfg.hostname + "\n");

    // locale + timezone
    cb(39, "Настройка локали...");
    write_file(MP + "/etc/locale.gen", "ru_RU.UTF-8 UTF-8\nen_US.UTF-8 UTF-8\n");
    sh("chroot " + MP + " locale-gen");
    write_file(MP + "/etc/locale.conf", "LANG=ru_RU.UTF-8\n");
    sh("chroot " + MP + " ln -sf /usr/share/zoneinfo/Europe/Moscow /etc/localtime");

    // установка пакетов
    cb(42, "Установка пакетов (ядро, X11, NetworkManager)...");
    write_file(MP + "/tmp/install-pkgs.sh",
        "#!/bin/bash\n"
        "export DEBIAN_FRONTEND=noninteractive\n"
        "apt-get update -q\n"
        "apt-get install -y --no-install-recommends \\\n"
        "  linux-image-amd64 grub-pc grub-efi-amd64 \\\n"
        "  xorg xinit x11-xserver-utils \\\n"
        "  libx11-dev libxext-dev xterm \\\n"
        "  network-manager net-tools \\\n"
        "  fonts-dejavu-core \\\n"
        "  sudo passwd libcrypt-dev \\\n"
        "  bash coreutils\n");
    sh("chmod +x " + MP + "/tmp/install-pkgs.sh");
    sh("chroot " + MP + " /tmp/install-pkgs.sh");

    // пользователь
    cb(70, "Создание пользователя...");
    sh("chroot " + MP + " useradd -m -s /bin/bash " + cfg.username);
    sh("echo '" + cfg.username + ":" + cfg.password +
       "' | chroot " + MP + " chpasswd");
    sh("chroot " + MP + " usermod -aG sudo " + cfg.username);

    // matos_login service user
    sh("chroot " + MP + " useradd -r -s /usr/local/bin/matos-login matos_login 2>/dev/null");

    // edition file
    cb(72, "Запись редакции...");
    write_file(MP + "/etc/matos-edition",
        std::string("EDITION=") + edition_name(cfg.edition) + "\n"
        "KEY=" + (cfg.edition == Edition::PRO ? "67691200" : "12006769") + "\n");

    // copy shell binaries
    cb(74, "Копирование компонентов MAT OS...");
    sh("cp /usr/local/bin/matos-shell " + MP + "/usr/local/bin/ 2>/dev/null || true");
    sh("cp /usr/local/bin/matos-login " + MP + "/usr/local/bin/ 2>/dev/null || true");
    sh("mkdir -p " + MP + "/opt/matos-shell/assets");
    sh("cp -r /opt/matos-shell/assets/* " + MP + "/opt/matos-shell/assets/ 2>/dev/null || true");

    // autologin getty
    cb(76, "Настройка автологина...");
    sh("mkdir -p " + MP + "/etc/systemd/system/getty@tty1.service.d");
    write_file(MP + "/etc/systemd/system/getty@tty1.service.d/autologin.conf",
        "[Service]\nExecStart=\nExecStart=-/sbin/agetty --autologin matos_login"
        " --noclear %I $TERM\n");
    sh("chroot " + MP + " systemctl enable NetworkManager 2>/dev/null || true");

    // xinitrc
    write_file(MP + "/home/" + cfg.username + "/.xinitrc",
        "#!/bin/sh\nexec /usr/local/bin/matos-shell\n");
    sh("chroot " + MP + " chown " + cfg.username + ":" + cfg.username +
       " /home/" + cfg.username + "/.xinitrc");
    sh("chmod +x " + MP + "/home/" + cfg.username + "/.xinitrc");

    // GRUB
    cb(82, "Установка загрузчика...");
    sh("mount --bind /dev "  + MP + "/dev");
    sh("mount --bind /proc " + MP + "/proc");
    sh("mount --bind /sys "  + MP + "/sys");
    if (cfg.uefi)
        sh("chroot " + MP + " grub-install --target=x86_64-efi --efi-directory=/boot/efi");
    else
        sh("chroot " + MP + " grub-install " + cfg.disk);
    sh("chroot " + MP + " update-grub");

    cb(95, "Размонтирование...");
    unmount_disk(MP);

    cb(100, "Установка завершена!");
    return true;
}
