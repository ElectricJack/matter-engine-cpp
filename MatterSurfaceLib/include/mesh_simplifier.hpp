#ifndef MESH_SIMPLIFIER_HPP
#define MESH_SIMPLIFIER_HPP

#include "raylib.h"
#include <cfloat>

// Options controlling QEM edge-collapse decimation.
struct SimplifyOptions {
    float target_ratio  = 0.5f;     // fraction of triangles to keep, (0..1]
    float max_error     = FLT_MAX;  // stop once min collapse cost exceeds this
    bool  lock_boundary = true;     // freeze vertices lying on a cell face plane
};

// Axis-aligned cell extent in cluster-local space. When supplied to
// simplify_mesh and lock_boundary is true, vertices on any of the 6 face
// planes are never moved or removed (guarantees watertight same-level seams).
struct CellBounds {
    Vector3 min_bound;
    Vector3 max_bound;
};

// Returns a NEW indexed Mesh allocated with raylib's allocator (MemAlloc),
// safe to pass to UploadMesh/UnloadMesh. Does NOT mutate or free `input`.
// On empty/degenerate input returns a zeroed Mesh (vertexCount == 0).
Mesh simplify_mesh(const Mesh& input,
                   const SimplifyOptions& opts,
                   const CellBounds* bounds = nullptr);

#endif // MESH_SIMPLIFIER_HPP
