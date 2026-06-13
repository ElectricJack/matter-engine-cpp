#include <cstdio>
#include <cassert>
#include <cmath>
#include <vector>

#include "raylib.h"
#include "mesh_simplifier.hpp"

// Build an indexed Mesh from raw vertex + index arrays (CPU-only, no GL upload).
static Mesh makeMesh(const std::vector<float>& v, const std::vector<unsigned short>& idx) {
    Mesh m = {0};
    m.vertexCount = (int)(v.size() / 3);
    m.triangleCount = (int)(idx.size() / 3);
    m.vertices = (float*)MemAlloc(sizeof(float) * v.size());
    for (size_t i = 0; i < v.size(); ++i) m.vertices[i] = v[i];
    m.indices = (unsigned short*)MemAlloc(sizeof(unsigned short) * idx.size());
    for (size_t i = 0; i < idx.size(); ++i) m.indices[i] = idx[i];
    return m;
}

// A flat n x n grid of quads on the z=0 plane spanning [0,span]x[0,span].
static Mesh makeGrid(int n /*cells per side*/, float span) {
    std::vector<float> v;
    std::vector<unsigned short> idx;
    int side = n + 1;
    for (int j = 0; j < side; ++j)
        for (int i = 0; i < side; ++i) {
            v.push_back(span * (float)i / (float)n);
            v.push_back(span * (float)j / (float)n);
            v.push_back(0.0f);
        }
    auto vid = [&](int i, int j) { return (unsigned short)(j*side + i); };
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i) {
            idx.push_back(vid(i, j));   idx.push_back(vid(i+1, j));   idx.push_back(vid(i+1, j+1));
            idx.push_back(vid(i, j));   idx.push_back(vid(i+1, j+1)); idx.push_back(vid(i, j+1));
        }
    return makeMesh(v, idx);
}

static void test_empty_input() {
    printf("=== test_empty_input ===\n");
    Mesh empty = {0};
    Mesh out = simplify_mesh(empty, SimplifyOptions{});
    assert(out.vertexCount == 0 && out.triangleCount == 0);
    printf("PASSED\n");
}

static void test_single_triangle() {
    printf("=== test_single_triangle ===\n");
    std::vector<float> v = {0,0,0, 1,0,0, 0,1,0};
    std::vector<unsigned short> idx = {0,1,2};
    Mesh in = makeMesh(v, idx);
    SimplifyOptions o; o.target_ratio = 0.1f; // ask for fewer, but 1 tri is the floor
    Mesh out = simplify_mesh(in, o);
    assert(out.triangleCount == 1);
    UnloadMesh(in); UnloadMesh(out);
    printf("PASSED\n");
}

static void test_identity_ratio_one() {
    printf("=== test_identity_ratio_one ===\n");
    Mesh in = makeGrid(2, 2.0f); // 9 verts, 8 tris
    SimplifyOptions o; o.target_ratio = 1.0f;
    Mesh out = simplify_mesh(in, o);
    assert(out.triangleCount == in.triangleCount);
    assert(out.vertexCount == in.vertexCount);
    UnloadMesh(in); UnloadMesh(out);
    printf("PASSED\n");
}

static void test_weld_non_indexed() {
    printf("=== test_weld_non_indexed ===\n");
    // Two triangles sharing an edge, expressed WITHOUT indices (6 verts, 2 shared).
    std::vector<float> v = {
        0,0,0, 1,0,0, 1,1,0,   // tri 0
        0,0,0, 1,1,0, 0,1,0    // tri 1 (shares 0,0,0 and 1,1,0)
    };
    Mesh in = {0};
    in.vertexCount = 6;
    in.triangleCount = 2;
    in.vertices = (float*)MemAlloc(sizeof(float) * v.size());
    for (size_t i = 0; i < v.size(); ++i) in.vertices[i] = v[i];
    // indices == NULL -> simplifier must weld
    SimplifyOptions o; o.target_ratio = 1.0f;
    Mesh out = simplify_mesh(in, o);
    assert(out.vertexCount == 4);   // welded down to 4 unique corners
    assert(out.triangleCount == 2);
    UnloadMesh(in); UnloadMesh(out);
    printf("PASSED\n");
}

static void test_normals_unit_length() {
    printf("=== test_normals_unit_length ===\n");
    Mesh in = makeGrid(2, 2.0f);
    Mesh out = simplify_mesh(in, SimplifyOptions{}); // ratio 0.5; with Task1 stub still valid mesh
    assert(out.normals != nullptr);
    for (int i = 0; i < out.vertexCount; ++i) {
        float nx = out.normals[i*3+0], ny = out.normals[i*3+1], nz = out.normals[i*3+2];
        float l = sqrtf(nx*nx + ny*ny + nz*nz);
        assert(fabsf(l - 1.0f) < 1e-3f);
    }
    UnloadMesh(in); UnloadMesh(out);
    printf("PASSED\n");
}

int main() {
    printf("=== Mesh Simplifier Tests ===\n");
    test_empty_input();
    test_single_triangle();
    test_identity_ratio_one();
    test_weld_non_indexed();
    test_normals_unit_length();
    printf("\nAll mesh simplifier scaffold tests PASSED\n");
    return 0;
}
