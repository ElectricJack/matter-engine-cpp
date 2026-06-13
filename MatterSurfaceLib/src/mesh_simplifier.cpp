#include "mesh_simplifier.hpp"

#include <vector>
#include <map>
#include <array>
#include <cmath>
#include <cstdint>

namespace {

// --- minimal double-precision vector helpers (avoids raymath coupling) ---
struct V3 { double x, y, z; };
static inline V3 sub(V3 a, V3 b)   { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static inline V3 cross(V3 a, V3 b) { return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x}; }
static inline double dot(V3 a, V3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline double len(V3 a)       { return std::sqrt(dot(a, a)); }

// --- accumulated quadric (Garland-Heckbert), used by the Task 2 engine ---
struct Quadric {
    double a00=0,a01=0,a02=0,a11=0,a12=0,a22=0; // symmetric 3x3 A
    double b0=0,b1=0,b2=0;                       // vector b
    double c=0;                                  // scalar c
    void addPlane(V3 n, double d) {
        a00+=n.x*n.x; a01+=n.x*n.y; a02+=n.x*n.z;
        a11+=n.y*n.y; a12+=n.y*n.z; a22+=n.z*n.z;
        b0+=d*n.x; b1+=d*n.y; b2+=d*n.z; c+=d*d;
    }
    void add(const Quadric& q) {
        a00+=q.a00; a01+=q.a01; a02+=q.a02; a11+=q.a11; a12+=q.a12; a22+=q.a22;
        b0+=q.b0; b1+=q.b1; b2+=q.b2; c+=q.c;
    }
    double error(V3 v) const {
        double e = a00*v.x*v.x + 2*a01*v.x*v.y + 2*a02*v.x*v.z
                 + a11*v.y*v.y + 2*a12*v.y*v.z + a22*v.z*v.z
                 + 2*(b0*v.x + b1*v.y + b2*v.z) + c;
        return e < 0 ? 0 : e;
    }
    bool optimal(V3& out) const {
        double m00 = a11*a22 - a12*a12;
        double m01 = a02*a12 - a01*a22;
        double m02 = a01*a12 - a02*a11;
        double det = a00*m00 + a01*m01 + a02*m02;
        if (std::fabs(det) < 1e-12) return false;
        double m11 = a00*a22 - a02*a02;
        double m12 = a01*a02 - a00*a12;
        double m22 = a00*a11 - a01*a01;
        double inv = 1.0 / det;
        out.x = -inv*(m00*b0 + m01*b1 + m02*b2);
        out.y = -inv*(m01*b0 + m11*b1 + m12*b2);
        out.z = -inv*(m02*b0 + m12*b1 + m22*b2);
        return true;
    }
};

struct WVert {
    V3 pos {0,0,0};
    Quadric q;
    bool locked = false;
    bool removed = false;
    int  version = 0;
};
struct WTri {
    int v[3] = {0,0,0};
    bool removed = false;
};

// Build working topology from the input mesh. Indexed input is read directly;
// non-indexed input is welded by exact-quantized position so edge-collapse has
// real connectivity.
static void buildTopology(const Mesh& m, std::vector<WVert>& verts, std::vector<WTri>& tris) {
    if (m.indices) {
        verts.resize(m.vertexCount);
        for (int i = 0; i < m.vertexCount; ++i) {
            verts[i].pos = {m.vertices[i*3+0], m.vertices[i*3+1], m.vertices[i*3+2]};
        }
        tris.resize(m.triangleCount);
        for (int t = 0; t < m.triangleCount; ++t) {
            tris[t].v[0] = m.indices[t*3+0];
            tris[t].v[1] = m.indices[t*3+1];
            tris[t].v[2] = m.indices[t*3+2];
        }
    } else {
        std::map<std::array<long long,3>, int> weld;
        tris.resize(m.triangleCount);
        for (int t = 0; t < m.triangleCount; ++t) {
            for (int k = 0; k < 3; ++k) {
                int src = t*3 + k;
                float x = m.vertices[src*3+0], y = m.vertices[src*3+1], z = m.vertices[src*3+2];
                std::array<long long,3> key = {
                    (long long)std::llround((double)x * 100000.0),
                    (long long)std::llround((double)y * 100000.0),
                    (long long)std::llround((double)z * 100000.0)
                };
                auto it = weld.find(key);
                int vi;
                if (it == weld.end()) {
                    vi = (int)verts.size();
                    WVert w; w.pos = {x, y, z};
                    verts.push_back(w);
                    weld[key] = vi;
                } else {
                    vi = it->second;
                }
                tris[t].v[k] = vi;
            }
        }
    }
}

// Compact surviving verts/tris into a new indexed Mesh with smooth
// area-weighted vertex normals (matches the marching-cubes convention).
static Mesh buildMesh(const std::vector<WVert>& verts, const std::vector<WTri>& tris) {
    Mesh out = {0};
    std::vector<int> remap(verts.size(), -1);
    int nv = 0;
    for (size_t i = 0; i < verts.size(); ++i)
        if (!verts[i].removed) remap[i] = nv++;

    std::vector<unsigned short> idx;
    for (const auto& tr : tris) {
        if (tr.removed) continue;
        idx.push_back((unsigned short)remap[tr.v[0]]);
        idx.push_back((unsigned short)remap[tr.v[1]]);
        idx.push_back((unsigned short)remap[tr.v[2]]);
    }
    int nt = (int)idx.size() / 3;
    if (nv == 0 || nt == 0) return out; // zeroed -> empty mesh

    out.vertexCount = nv;
    out.triangleCount = nt;
    out.vertices = (float*)MemAlloc(sizeof(float) * 3 * nv);
    out.normals  = (float*)MemAlloc(sizeof(float) * 3 * nv);
    out.indices  = (unsigned short*)MemAlloc(sizeof(unsigned short) * idx.size());

    for (size_t i = 0; i < verts.size(); ++i) {
        if (verts[i].removed) continue;
        int j = remap[i];
        out.vertices[j*3+0] = (float)verts[i].pos.x;
        out.vertices[j*3+1] = (float)verts[i].pos.y;
        out.vertices[j*3+2] = (float)verts[i].pos.z;
    }
    for (size_t i = 0; i < idx.size(); ++i) out.indices[i] = idx[i];

    for (int i = 0; i < nv*3; ++i) out.normals[i] = 0.0f;
    for (int t = 0; t < nt; ++t) {
        int a = idx[t*3+0], b = idx[t*3+1], c = idx[t*3+2];
        V3 va = {out.vertices[a*3+0], out.vertices[a*3+1], out.vertices[a*3+2]};
        V3 vb = {out.vertices[b*3+0], out.vertices[b*3+1], out.vertices[b*3+2]};
        V3 vc = {out.vertices[c*3+0], out.vertices[c*3+1], out.vertices[c*3+2]};
        V3 n = cross(sub(vb, va), sub(vc, va)); // area-weighted (twice area)
        int ix[3] = {a, b, c};
        for (int k = 0; k < 3; ++k) {
            out.normals[ix[k]*3+0] += (float)n.x;
            out.normals[ix[k]*3+1] += (float)n.y;
            out.normals[ix[k]*3+2] += (float)n.z;
        }
    }
    for (int i = 0; i < nv; ++i) {
        V3 nn = {out.normals[i*3+0], out.normals[i*3+1], out.normals[i*3+2]};
        double l = len(nn);
        if (l > 1e-12) {
            out.normals[i*3+0] /= (float)l;
            out.normals[i*3+1] /= (float)l;
            out.normals[i*3+2] /= (float)l;
        } else {
            out.normals[i*3+0] = 0; out.normals[i*3+1] = 1; out.normals[i*3+2] = 0;
        }
    }
    return out;
}

// Decimation engine. STUB in Task 1 (does nothing); implemented in Task 2.
static void decimate(std::vector<WVert>& verts, std::vector<WTri>& tris,
                     const SimplifyOptions& opts, const CellBounds* bounds, int inputTri) {
    (void)verts; (void)tris; (void)opts; (void)bounds; (void)inputTri;
}

} // anonymous namespace

Mesh simplify_mesh(const Mesh& input, const SimplifyOptions& opts, const CellBounds* bounds) {
    if (input.vertexCount == 0 || input.triangleCount == 0 || !input.vertices) {
        Mesh empty = {0};
        return empty;
    }
    std::vector<WVert> verts;
    std::vector<WTri> tris;
    buildTopology(input, verts, tris);
    int inputTri = (int)tris.size();
    decimate(verts, tris, opts, bounds, inputTri);
    return buildMesh(verts, tris);
}
