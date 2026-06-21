#include "../include/part_asset.h"

#include <cstdio>

namespace part_asset {

uint64_t fnv1a64(const void* data, size_t len) {
    uint64_t h = 1469598103934665603ull;            // FNV offset basis
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

uint64_t compute_param_hash(const PartGenParams& p) {
    return fnv1a64(&p, sizeof(p)) ^ static_cast<uint64_t>(kFormatVersion);
}

std::string cache_path(uint64_t hash) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(hash));
    return std::string("parts/") + buf + ".part";
}

} // namespace part_asset
