#include "disk.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    while (!s.empty() && (s.back() == '\n' || s.back() == ' '))
        s.pop_back();
    return s;
}

static std::string human_size(uint64_t b) {
    char buf[32];
    if      (b >= 1000000000ULL) snprintf(buf,sizeof(buf),"%.0f GB", b/1e9);
    else if (b >= 1000000ULL)    snprintf(buf,sizeof(buf),"%.0f MB", b/1e6);
    else                         snprintf(buf,sizeof(buf),"%.0f KB", b/1e3);
    return buf;
}

std::vector<DiskInfo> list_disks() {
    std::vector<DiskInfo> result;
    DIR* d = opendir("/sys/block");
    if (!d) return result;

    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        std::string name = ent->d_name;
        if (name[0] == '.') continue;
        // пропускаем loop, ram и т.п.
        if (name.find("loop") == 0 || name.find("ram") == 0 ||
            name.find("zram") == 0) continue;

        std::string base = "/sys/block/" + name;
        std::string size_str = read_file(base + "/size");
        if (size_str.empty()) continue;

        uint64_t sectors = std::stoull(size_str);
        uint64_t bytes   = sectors * 512;
        if (bytes < 1000000000ULL) continue; // < 1 ГБ — пропускаем

        DiskInfo di;
        di.path       = "/dev/" + name;
        di.model      = read_file(base + "/device/model");
        if (di.model.empty()) di.model = name;
        di.size_bytes = bytes;
        di.size_human = human_size(bytes);
        result.push_back(di);
    }
    closedir(d);
    return result;
}

static int run(const std::string& cmd) {
    return system(cmd.c_str());
}

bool partition_disk(const PartitionPlan& plan) {
    if (plan.uefi) {
        return run("parted -s " + plan.disk +
                   " mklabel gpt"
                   " mkpart primary fat32 1MiB 513MiB"
                   " set 1 esp on"
                   " mkpart primary ext4 513MiB 100%") == 0;
    } else {
        return run("parted -s " + plan.disk +
                   " mklabel msdos"
                   " mkpart primary ext4 1MiB 100%"
                   " set 1 boot on") == 0;
    }
}

bool format_disk(const std::string& disk, bool uefi) {
    if (uefi) {
        if (run("mkfs.vfat -F32 " + disk + "1") != 0) return false;
        return run("mkfs.ext4 -F "  + disk + "2") == 0;
    } else {
        return run("mkfs.ext4 -F " + disk + "1") == 0;
    }
}

bool mount_disk(const std::string& disk, const std::string& mp, bool uefi) {
    run("mkdir -p " + mp);
    if (uefi) {
        if (run("mount " + disk + "2 " + mp) != 0) return false;
        run("mkdir -p " + mp + "/boot/efi");
        return run("mount " + disk + "1 " + mp + "/boot/efi") == 0;
    } else {
        return run("mount " + disk + "1 " + mp) == 0;
    }
}

bool unmount_disk(const std::string& mp) {
    run("umount -lf " + mp + "/boot/efi 2>/dev/null");
    run("umount -lf " + mp + "/proc 2>/dev/null");
    run("umount -lf " + mp + "/sys 2>/dev/null");
    run("umount -lf " + mp + "/dev 2>/dev/null");
    return run("umount -lf " + mp) == 0;
}
