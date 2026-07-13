#pragma once
#include <string>
#include <vector>

struct DiskInfo {
    std::string path;       // /dev/sda
    std::string model;
    uint64_t    size_bytes;
    std::string size_human; // "120 GB"
};

std::vector<DiskInfo> list_disks();

struct PartitionPlan {
    std::string disk;       // /dev/sda
    bool        uefi;       // true = GPT+EFI, false = MBR
};

bool partition_disk(const PartitionPlan& plan);
bool format_disk(const std::string& disk, bool uefi);
bool mount_disk(const std::string& disk, const std::string& mountpoint, bool uefi);
bool unmount_disk(const std::string& mountpoint);
