#include "license.h"

static const char* KEY_PRO  = "67691200";
static const char* KEY_HOME = "12006769";

LicenseResult check_license(const std::string& key) {
    if (key == KEY_PRO)  return { true, Edition::PRO };
    if (key == KEY_HOME) return { true, Edition::HOME };
    return { false, Edition::NONE };
}

const char* edition_name(Edition e) {
    switch (e) {
        case Edition::PRO:  return "MAT OS PRO";
        case Edition::HOME: return "MAT OS HOME";
        default:            return "Unknown";
    }
}
