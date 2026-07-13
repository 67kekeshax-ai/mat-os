#pragma once
#include <string>

enum class Edition { NONE, HOME, PRO };

struct LicenseResult {
    bool    valid;
    Edition edition;
};

LicenseResult check_license(const std::string& key);
const char*   edition_name(Edition e);
