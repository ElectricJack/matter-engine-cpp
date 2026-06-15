#pragma once

#include "lattice.h"
#include "occupancy.h"
#include <vector>
#include <cstdint>

// A particle ready to hand to Cluster::add_particle (jitter already applied).
struct EmittedParticle {
    Vector3 position;   // local-space
    float radius;
    uint32_t materialId;
    Vector4 tint;       // RGBA; w = blend strength
};

struct CullParams {
    int margin;          // sub-shell layers to keep; clamped to >= 1
    float base_radius;   // nominal particle radius
    float jitter_amount; // per-axis position jitter magnitude (0 = none)
    float tint_alpha;    // tint blend strength written to EmittedParticle.tint.w
    uint32_t seed;       // determinism seed for jitter/tint
    float cell_size;     // meshing cell size used to bucket slots into cells
    Vector3 cell_origin_offset; // added to slot_position before bucketing so the
                                // cull grid matches the Cluster's recentered grid
};

// Per-call statistics about the cell-granular decision (optional output).
struct CullStats {
    size_t cells_total = 0;
    size_t cells_kept = 0;
    size_t cells_dropped = 0;
};

// Deterministic value-noise primitives (moved from main.cpp). [0,1] output.
float lattice_vhash(int x, int y, int z);
float lattice_vnoise(float x, float y, float z);

// A slot is buried iff every slot in the Chebyshev box of half-width `margin`
// around it is occupied. This currently hardcodes the grid's box neighborhood
// and does not consult lattice.neighbor_offsets(); future non-grid lattices
// would instead expand neighbor_offsets via BFS to `margin` steps.
bool slot_is_buried(const Occupancy& occ, SlotCoord c, int margin);

// Cell-granular interior culling. Slots are bucketed into meshing cells via
// floor((slot_position + cell_origin_offset) / cell_size). A cell is KEPT if any
// of its slots is non-buried (slot_is_buried with margin>=1); a cell is DROPPED
// only when EVERY slot in it is buried. Kept cells emit ALL their slots, so no
// holes (and thus no inner SDF cavity) ever form inside a meshed cell. When
// `stats` is non-null it is filled with the per-call cell counts.
std::vector<EmittedParticle> cull_interior(const Lattice& lattice,
                                           const Occupancy& occ,
                                           const CullParams& p,
                                           CullStats* stats = nullptr);

// Baseline: emit a particle for every occupied slot (no culling). Used by the
// A/B acceptance comparison.
std::vector<EmittedParticle> emit_all(const Lattice& lattice,
                                      const Occupancy& occ,
                                      const CullParams& p);
