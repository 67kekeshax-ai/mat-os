#pragma once
#include "../license/license.h"
#include <string>
#include <functional>

struct InstallConfig {
    std::string disk;           // /dev/sda
    bool        uefi = false;
    std::string username;
    std::string password;
    std::string hostname = "matos-pc";
    Edition     edition  = Edition::HOME;
};

// callback(step 0-100, message)
using ProgressCallback = std::function<void(int, const std::string&)>;

bool run_install(const InstallConfig& cfg, ProgressCallback progress);
