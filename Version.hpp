#pragma once

// Canonical Lyra version metadata. CMake reads the numeric fields to set the
// project version; the executable uses the string fields for display/reporting.
#define LYRA_VERSION_MAJOR 0
#define LYRA_VERSION_MINOR 2
#define LYRA_VERSION_PATCH 0
#define LYRA_VERSION_CODENAME "Cloudsdale"
#define LYRA_PROJECT_NAME "Lyra"

#define LYRA_DETAIL_STRINGIFY(value) #value
#define LYRA_STRINGIFY(value) LYRA_DETAIL_STRINGIFY(value)
#define LYRA_VERSION_STRING \
    LYRA_STRINGIFY(LYRA_VERSION_MAJOR) "." \
    LYRA_STRINGIFY(LYRA_VERSION_MINOR) "." \
    LYRA_STRINGIFY(LYRA_VERSION_PATCH)

namespace lyra::version {
inline constexpr unsigned int major = LYRA_VERSION_MAJOR;
inline constexpr unsigned int minor = LYRA_VERSION_MINOR;
inline constexpr unsigned int patch = LYRA_VERSION_PATCH;
inline constexpr char projectName[] = LYRA_PROJECT_NAME;
inline constexpr char codename[] = LYRA_VERSION_CODENAME;
inline constexpr char string[] = LYRA_VERSION_STRING;
}
