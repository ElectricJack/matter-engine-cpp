#include "../include/csg_lowering.h"
#include <cmath>

// NOTE: raymath.h cannot be included in this TU: cluster.h transitively pulls in
// the prototype's precomp.h `struct float3` (x/y/z members) which collides with
// raymath's `struct float3 {float v[3];}`. The few matrix ops needed here are
// implemented directly on raylib's column-major Matrix.

namespace dsl {

// raylib Matrix is column-major: m0..m3 col0, m4..m7 col1, etc. m12,m13,m14 =
// translation. Transform a point as M * (p,1).
static Vector3 xf(const Matrix& m, const Vector3& p) {
    Vector3 o;
    o.x = m.m0*p.x + m.m4*p.y + m.m8 *p.z + m.m12;
    o.y = m.m1*p.x + m.m5*p.y + m.m9 *p.z + m.m13;
    o.z = m.m2*p.x + m.m6*p.y + m.m10*p.z + m.m14;
    return o;
}

// Stamp a box as a pack of spheres at step = spacing/2 so the SDF surface (incl.
// sharp corners) is resolved to the resolution floor. Radius ~ step so spheres
// overlap and the union reads as a solid box to the mesher.
template <class Emit>
static void stamp_box(const BuildOp& o, Emit emit) {
    float step = (o.spacing > 0 ? o.spacing : 0.1f) * 0.5f;
    Vector3 h = o.halfExtents;
    for (float x=-h.x; x<=h.x+1e-4f; x+=step)
      for (float y=-h.y; y<=h.y+1e-4f; y+=step)
        for (float z=-h.z; z<=h.z+1e-4f; z+=step) {
            Vector3 local = { o.center.x+x, o.center.y+y, o.center.z+z };
            emit(xf(o.transform, local), step * 1.05f);
        }
    // Guarantee at least one sample for boxes smaller than a step (sub-min feature).
    if (h.x < step && h.y < step && h.z < step)
        emit(xf(o.transform, o.center), std::max(step, std::max(h.x,std::max(h.y,h.z)))*1.05f);
}

LoweredField lower_build_buffer(const BuildBuffer& buf) {
    LoweredField out;
    for (const BuildOp& o : buf.ops) {
        out.smoothing = std::max(out.smoothing, o.smoothing);
        bool subtract = (o.op == CsgOp::Difference);
        if (o.kind == BrushKind::Sphere) {
            Vector3 c = xf(o.transform, o.center);
            if (subtract) { Particle p{ c, o.radius, (int)o.materialId }; out.carve.push_back(p); }
            else { out.additive.push_back(StaticParticle(c, o.radius, o.materialId,
                       {1,1,1,0}, o.spacing)); }
        } else { // Box
            if (subtract) stamp_box(o, [&](Vector3 c, float r){ out.carve.push_back(Particle{c,r,(int)o.materialId}); });
            else stamp_box(o, [&](Vector3 c, float r){ out.additive.push_back(StaticParticle(c,r,o.materialId,{1,1,1,0},o.spacing)); });
        }
    }
    return out;
}

} // namespace dsl
