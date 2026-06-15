#include "particle_culling.h"
#include <cmath>
#include <unordered_map>
#include <cassert>

float lattice_vhash(int x, int y, int z) {
    uint32_t h = ((uint32_t)x * 374761393u) ^ ((uint32_t)y * 668265263u) ^ ((uint32_t)z * 2147483647u);
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFFFFu) / (float)0xFFFFFFu; // [0,1]
}

float lattice_vnoise(float x, float y, float z) {
    int xi = (int)floorf(x), yi = (int)floorf(y), zi = (int)floorf(z);
    float xf = x - xi, yf = y - yi, zf = z - zi;
    auto lerpf  = [](float a, float b, float t) { return a + (b - a) * t; };
    auto smooth = [](float t) { return t * t * (3.0f - 2.0f * t); };
    float u = smooth(xf), v = smooth(yf), w = smooth(zf);
    float c000 = lattice_vhash(xi, yi, zi),     c100 = lattice_vhash(xi+1, yi, zi);
    float c010 = lattice_vhash(xi, yi+1, zi),   c110 = lattice_vhash(xi+1, yi+1, zi);
    float c001 = lattice_vhash(xi, yi, zi+1),   c101 = lattice_vhash(xi+1, yi, zi+1);
    float c011 = lattice_vhash(xi, yi+1, zi+1), c111 = lattice_vhash(xi+1, yi+1, zi+1);
    float x00 = lerpf(c000, c100, u), x10 = lerpf(c010, c110, u);
    float x01 = lerpf(c001, c101, u), x11 = lerpf(c011, c111, u);
    return lerpf(lerpf(x00, x10, v), lerpf(x01, x11, v), w); // [0,1]
}

bool slot_is_buried(const Occupancy& occ, SlotCoord c, int margin) {
    for (int dz = -margin; dz <= margin; ++dz)
    for (int dy = -margin; dy <= margin; ++dy)
    for (int dx = -margin; dx <= margin; ++dx) {
        if (!occ.occupied(SlotCoord{c.x + dx, c.y + dy, c.z + dz})) return false;
    }
    return true;
}

// Build one emitted particle for a slot. Jitter and tint are pure functions of
// (SlotCoord, seed) so the same design always bakes identically.
static EmittedParticle make_particle(const Lattice& lat, SlotCoord c,
                                     const SlotData& d, const CullParams& p) {
    Vector3 base = lat.slot_position(c);
    int s = (int)p.seed;
    float jx = (lattice_vhash(c.x * 2 + 1 + s, c.y, c.z) - 0.5f) * p.jitter_amount;
    float jy = (lattice_vhash(c.x, c.y * 2 + 1 + s, c.z) - 0.5f) * p.jitter_amount;
    float jz = (lattice_vhash(c.x, c.y, c.z * 2 + 1 + s) - 0.5f) * p.jitter_amount;

    EmittedParticle ep;
    ep.position   = Vector3{ base.x + jx, base.y + jy, base.z + jz };
    ep.radius     = p.base_radius;
    ep.materialId = d.materialId;
    float tr = lattice_vhash(c.x + 101 + s, c.y, c.z);
    float tg = lattice_vhash(c.x, c.y + 101 + s, c.z);
    float tb = lattice_vhash(c.x, c.y, c.z + 101 + s);
    ep.tint = Vector4{ tr, tg, tb, p.tint_alpha };
    return ep;
}

// Pack a slot's owning meshing-cell coordinate into one key. Mirrors
// Cluster::get_cell_coordinates: floor((slot_pos + offset) / cell_size).
static uint64_t cell_key(const Lattice& lat, SlotCoord c, const CullParams& p) {
    Vector3 base = lat.slot_position(c);
    int cx = (int)floorf((base.x + p.cell_origin_offset.x) / p.cell_size);
    int cy = (int)floorf((base.y + p.cell_origin_offset.y) / p.cell_size);
    int cz = (int)floorf((base.z + p.cell_origin_offset.z) / p.cell_size);
    return pack_slot(SlotCoord{cx, cy, cz});
}

std::vector<EmittedParticle> cull_interior(const Lattice& lattice,
                                           const Occupancy& occ,
                                           const CullParams& p,
                                           CullStats* stats) {
    assert(p.cell_size > 0.0f && "CullParams.cell_size must be positive");
    int margin = p.margin < 1 ? 1 : p.margin;

    // Pass 1: decide keep/drop per cell. A cell is kept iff any of its slots is
    // non-buried; this is independent of iteration order.
    std::unordered_map<uint64_t, bool> keep;
    occ.for_each([&](SlotCoord c, const SlotData&) {
        uint64_t k = cell_key(lattice, c, p);
        bool not_buried = !slot_is_buried(occ, c, margin);
        auto it = keep.find(k);
        if (it == keep.end()) keep.emplace(k, not_buried);
        else if (not_buried) it->second = true;
    });

    // Pass 2: emit every slot of every kept cell.
    std::vector<EmittedParticle> out;
    occ.for_each([&](SlotCoord c, const SlotData& d) {
        auto it = keep.find(cell_key(lattice, c, p));
        if (it != keep.end() && it->second)
            out.push_back(make_particle(lattice, c, d, p));
    });

    if (stats) {
        stats->cells_total = keep.size();
        stats->cells_kept = 0;
        for (const auto& kv : keep) if (kv.second) ++stats->cells_kept;
        stats->cells_dropped = stats->cells_total - stats->cells_kept;
    }
    return out;
}

std::vector<EmittedParticle> emit_all(const Lattice& lattice,
                                      const Occupancy& occ,
                                      const CullParams& p) {
    std::vector<EmittedParticle> out;
    occ.for_each([&](SlotCoord c, const SlotData& d) {
        out.push_back(make_particle(lattice, c, d, p));
    });
    return out;
}
