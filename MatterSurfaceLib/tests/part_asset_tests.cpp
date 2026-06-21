#include "../include/part_asset.h"
#include <cstdio>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); ++failures; } } while (0)

static part_asset::PartGenParams sample_params() {
    part_asset::PartGenParams p{};
    p.dimX = 20; p.dimY = 20; p.dimZ = 20;
    p.spacing = 0.8f; p.baseRadius = 0.62f;
    p.posJitter = 0.1f; p.radiusVar = 0.1f; p.voidAmt = 0.05f;
    p.veinFreq = 1.5f; p.veinThresh = 0.3f;
    p.matOpaqueA = 8; p.matOpaqueB = 9; p.matGlass = 4;
    p.simplifyRatio = 0.65f; p.seed = 1234u;
    return p;
}

int main() {
    using namespace part_asset;

    // fnv1a64 is deterministic and order-sensitive.
    const char* a = "hello"; const char* b = "hellp";
    CHECK(fnv1a64(a, 5) == fnv1a64(a, 5), "fnv deterministic");
    CHECK(fnv1a64(a, 5) != fnv1a64(b, 5), "fnv distinguishes input");

    // compute_param_hash: same params -> same hash; changed field -> different.
    PartGenParams p1 = sample_params();
    PartGenParams p2 = sample_params();
    CHECK(compute_param_hash(p1) == compute_param_hash(p2), "same params same hash");
    p2.seed = 9999u;
    CHECK(compute_param_hash(p1) != compute_param_hash(p2), "seed change rehashes");
    p2 = sample_params(); p2.simplifyRatio = 0.5f;
    CHECK(compute_param_hash(p1) != compute_param_hash(p2), "ratio change rehashes");

    // cache_path format.
    CHECK(cache_path(0x1ull) == "parts/0000000000000001.part", "cache_path zero-padded hex");

    if (failures == 0) printf("All part_asset tests passed\n");
    return failures == 0 ? 0 : 1;
}
