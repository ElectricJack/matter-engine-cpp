extern "C" {
    #include "raylib.h"
    #include "rlgl.h"
}

// Minimal helper: raymath.h conflicts with precomp.h's float3 type, so we
// define only what we need here.
static inline Matrix ImposterMatrixIdentity(void) {
    Matrix result = { 1.0f, 0.0f, 0.0f, 0.0f,
                      0.0f, 1.0f, 0.0f, 0.0f,
                      0.0f, 0.0f, 1.0f, 0.0f,
                      0.0f, 0.0f, 0.0f, 1.0f };
    return result;
}
#include "../include/imposter_asset.h"
#include "../include/blas_manager.hpp"
#include "../include/tlas_manager.hpp"
#include <vector>

namespace imposter_asset {

bool bake_imposter(const ImpGenParams& p, const std::vector<Tri>& part_tris,
                   uint64_t source_part_hash,
                   BLASManager& blas, TLASManager& tlas, ImposterAsset& out) {
    if (!build_cage(part_tris, p, source_part_hash, out)) return false;
    if (!bake_displacement_cpu(part_tris, out)) return false;

    const int vc = (int)out.verts.size();
    Mesh cage{};
    cage.vertexCount = vc;
    cage.triangleCount = (int)out.tris.size();
    cage.vertices  = (float*)MemAlloc(sizeof(float)*3*vc);
    cage.normals   = (float*)MemAlloc(sizeof(float)*3*vc);
    cage.texcoords = (float*)MemAlloc(sizeof(float)*2*vc);
    for (int i=0;i<vc;++i) {
        cage.vertices[i*3+0]=out.verts[i].px; cage.vertices[i*3+1]=out.verts[i].py; cage.vertices[i*3+2]=out.verts[i].pz;
        cage.normals[i*3+0]=out.verts[i].nx;  cage.normals[i*3+1]=out.verts[i].ny;  cage.normals[i*3+2]=out.verts[i].nz;
        cage.texcoords[i*2+0]=out.verts[i].u; cage.texcoords[i*2+1]=out.verts[i].v;
    }
    UploadMesh(&cage, false);

    Shader bake = LoadShader("shaders/imposter_bake.vs", "shaders/imposter_bake_processed.fs");
    RenderTexture2D rt = LoadRenderTexture((int)out.atlas_w, (int)out.atlas_h);

    blas.bind_to_shader(bake);
    tlas.bind_to_shader(bake, blas);
    int maxDispLoc = GetShaderLocation(bake, "maxDisp");
    SetShaderValue(bake, maxDispLoc, &out.max_disp, SHADER_UNIFORM_FLOAT);
    int modeLoc = GetShaderLocation(bake, "intersectionMode"); int one = 1;
    SetShaderValue(bake, modeLoc, &one, SHADER_UNIFORM_INT);
    float fOne = 1.0f;
    SetShaderValue(bake, GetShaderLocation(bake, "giStrength"), &fOne, SHADER_UNIFORM_FLOAT);
    SetShaderValue(bake, GetShaderLocation(bake, "shadowStrength"), &fOne, SHADER_UNIFORM_FLOAT);
    int aoOn = 1; SetShaderValue(bake, GetShaderLocation(bake, "aoEnabled"), &aoOn, SHADER_UNIFORM_INT);

    Material mat = LoadMaterialDefault();
    mat.shader = bake;

    BeginTextureMode(rt);
        ClearBackground(BLANK);
        rlDisableBackfaceCulling();
        rlDisableDepthTest();
        DrawMesh(cage, mat, ImposterMatrixIdentity());
        rlEnableDepthTest();
    EndTextureMode();

    Image img = LoadImageFromTexture(rt.texture);     // RGBA8, y-flipped
    ImageFlipVertical(&img);
    const unsigned char* px = (const unsigned char*)img.data;
    const int W=(int)out.atlas_w, H=(int)out.atlas_h;
    for (int i=0;i<W*H;++i) {
        out.color[i*4+0]=px[i*4+0];
        out.color[i*4+1]=px[i*4+1];
        out.color[i*4+2]=px[i*4+2];
        // out.color[i*4+3] keeps CPU coverage from bake_displacement_cpu
    }
    UnloadImage(img);

    dilate_atlas(out, 4);

    UnloadRenderTexture(rt);
    UnloadShader(bake);
    // We already unloaded `bake`; reset to the default shader id so UnloadMaterial's
    // `id != default` guard skips it and only frees mat.maps (no glDeleteProgram(0)).
    mat.shader.id = rlGetShaderIdDefault();
    UnloadMaterial(mat);
    UnloadMesh(cage);
    return true;
}

std::vector<Tri> flatten_part_triangles(const BLASManager& blas, const TLASManager& tlas) {
    std::vector<Tri> out;
    const auto& recs = tlas.get_draw_records();
    for (const auto& r : recs) {
        const BLASManager::BLASEntry* e = blas.get_entry(r.blas_handle);
        if (!e) continue;
        const float* m = r.transform.m; // row-major 4x4
        auto xf = [&](float3 p){
            return make_float3(
                m[0]*p.x + m[1]*p.y + m[2]*p.z + m[3],
                m[4]*p.x + m[5]*p.y + m[6]*p.z + m[7],
                m[8]*p.x + m[9]*p.y + m[10]*p.z + m[11]);
        };
        for (const Tri& t : e->triangles) {
            Tri w;
            w.vertex0 = xf(t.vertex0); w.vertex1 = xf(t.vertex1); w.vertex2 = xf(t.vertex2);
            w.centroid = make_float3((w.vertex0.x+w.vertex1.x+w.vertex2.x)/3.0f,
                                     (w.vertex0.y+w.vertex1.y+w.vertex2.y)/3.0f,
                                     (w.vertex0.z+w.vertex1.z+w.vertex2.z)/3.0f);
            out.push_back(w);
        }
    }
    return out;
}

std::vector<Tri> cage_to_tris(const ImposterAsset& a) {
    std::vector<Tri> out; out.reserve(a.tris.size());
    for (const auto& ct : a.tris) {
        const CageVert& A=a.verts[ct.i0]; const CageVert& B=a.verts[ct.i1]; const CageVert& C=a.verts[ct.i2];
        Tri t;
        t.vertex0=make_float3(A.px,A.py,A.pz);
        t.vertex1=make_float3(B.px,B.py,B.pz);
        t.vertex2=make_float3(C.px,C.py,C.pz);
        t.centroid=make_float3((A.px+B.px+C.px)/3.0f,(A.py+B.py+C.py)/3.0f,(A.pz+B.pz+C.pz)/3.0f);
        out.push_back(t);
    }
    return out;
}

} // namespace imposter_asset
