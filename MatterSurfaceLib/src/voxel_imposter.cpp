#include "../include/voxel_imposter.h"
#include "../include/part_asset.h"   // fnv1a64 (used in later tasks)
#include <algorithm>
#include <cmath>
#include <cstring>
// tlas_manager.hpp and blas_manager.hpp are already pulled in via voxel_imposter.h

namespace {
inline void sub(float r[3],const float a[3],const float b[3]){ r[0]=a[0]-b[0];r[1]=a[1]-b[1];r[2]=a[2]-b[2]; }
inline void cross(float r[3],const float a[3],const float b[3]){ r[0]=a[1]*b[2]-a[2]*b[1]; r[1]=a[2]*b[0]-a[0]*b[2]; r[2]=a[0]*b[1]-a[1]*b[0]; }
inline float dot(const float a[3],const float b[3]){ return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
inline bool plane_box_overlap(const float n[3],float d,const float h[3]){
    float vmin[3],vmax[3];
    for(int q=0;q<3;++q){ if(n[q]>0){vmin[q]=-h[q];vmax[q]=h[q];}else{vmin[q]=h[q];vmax[q]=-h[q];} }
    if(dot(n,vmin)+d>0) return false;
    if(dot(n,vmax)+d>=0) return true;
    return false;
}
} // namespace

namespace voxel_imposter {

bool choose_grid_dims(const float lo[3], const float hi[3],
                      int maxDim, int& nx, int& ny, int& nz) {
    float ex = hi[0]-lo[0], ey = hi[1]-lo[1], ez = hi[2]-lo[2];
    float maxExtent = std::max(ex, std::max(ey, ez));
    if (maxExtent <= 0.0f || maxDim < 1) return false;
    float v = maxExtent / (float)maxDim;
    auto dim = [&](float e){ int d = (int)std::ceil(e / v); return std::max(1, std::min(maxDim, d)); };
    nx = dim(ex); ny = dim(ey); nz = dim(ez);
    return true;
}

bool tri_box_overlap(const float bc[3], const float bh[3],
                     const float V0[3], const float V1[3], const float V2[3]) {
    float v0[3],v1[3],v2[3]; sub(v0,V0,bc); sub(v1,V1,bc); sub(v2,V2,bc);
    float e0[3],e1[3],e2[3]; sub(e0,v1,v0); sub(e1,v2,v1); sub(e2,v0,v2);
    float fex,fey,fez,p0,p1,p2,rad,minv,maxv;
    (void)p2;
    #define AXISTEST_X(a,b,fa,fb,va,vb) \
        p0=a*va[1]-b*va[2]; p1=a*vb[1]-b*vb[2]; \
        minv=p0<p1?p0:p1; maxv=p0<p1?p1:p0; rad=fa*bh[1]+fb*bh[2]; \
        if(minv>rad||maxv<-rad) return false;
    #define AXISTEST_Y(a,b,fa,fb,va,vb) \
        p0=-a*va[0]+b*va[2]; p1=-a*vb[0]+b*vb[2]; \
        minv=p0<p1?p0:p1; maxv=p0<p1?p1:p0; rad=fa*bh[0]+fb*bh[2]; \
        if(minv>rad||maxv<-rad) return false;
    #define AXISTEST_Z(a,b,fa,fb,va,vb) \
        p0=a*va[0]-b*va[1]; p1=a*vb[0]-b*vb[1]; \
        minv=p0<p1?p0:p1; maxv=p0<p1?p1:p0; rad=fa*bh[0]+fb*bh[1]; \
        if(minv>rad||maxv<-rad) return false;
    fex=std::fabs(e0[0]);fey=std::fabs(e0[1]);fez=std::fabs(e0[2]);
    AXISTEST_X(e0[2],e0[1],fez,fey,v0,v2); AXISTEST_Y(e0[2],e0[0],fez,fex,v0,v2); AXISTEST_Z(e0[1],e0[0],fey,fex,v1,v2);
    fex=std::fabs(e1[0]);fey=std::fabs(e1[1]);fez=std::fabs(e1[2]);
    AXISTEST_X(e1[2],e1[1],fez,fey,v0,v2); AXISTEST_Y(e1[2],e1[0],fez,fex,v0,v2); AXISTEST_Z(e1[1],e1[0],fey,fex,v0,v1);
    fex=std::fabs(e2[0]);fey=std::fabs(e2[1]);fez=std::fabs(e2[2]);
    AXISTEST_X(e2[2],e2[1],fez,fey,v0,v1); AXISTEST_Y(e2[2],e2[0],fez,fex,v0,v1); AXISTEST_Z(e2[1],e2[0],fey,fex,v1,v2);
    #undef AXISTEST_X
    #undef AXISTEST_Y
    #undef AXISTEST_Z
    auto mm=[&](float a,float b,float c,float&mn,float&mx){ mn=mx=a; if(b<mn)mn=b; if(b>mx)mx=b; if(c<mn)mn=c; if(c>mx)mx=c; };
    mm(v0[0],v1[0],v2[0],minv,maxv); if(minv>bh[0]||maxv<-bh[0]) return false;
    mm(v0[1],v1[1],v2[1],minv,maxv); if(minv>bh[1]||maxv<-bh[1]) return false;
    mm(v0[2],v1[2],v2[2],minv,maxv); if(minv>bh[2]||maxv<-bh[2]) return false;
    float n[3]; cross(n,e0,e1); float d=-dot(n,v0);
    return plane_box_overlap(n,d,bh);
}

void oct_encode(const float n[3], uint8_t out[2]) {
    double ax=std::fabs((double)n[0])+std::fabs((double)n[1])+std::fabs((double)n[2]);
    double x=n[0]/ax, y=n[1]/ax;
    if (n[2] < 0.0f) {
        double ox=(1.0-std::fabs(y))*(x>=0?1.0:-1.0);
        double oy=(1.0-std::fabs(x))*(y>=0?1.0:-1.0);
        x=ox; y=oy;
    }
    auto q=[](double v){ v=0.5*(v+1.0); v=v<0?0:(v>1?1:v); return (uint8_t)(v*255.0+0.5); };
    out[0]=q(x); out[1]=q(y);
}
void oct_decode(const uint8_t in[2], float n[3]) {
    float x=in[0]/255.0f*2.0f-1.0f, y=in[1]/255.0f*2.0f-1.0f;
    float z=1.0f-std::fabs(x)-std::fabs(y);
    if (z<0.0f) { float ox=(1.0f-std::fabs(y))*(x>=0?1.0f:-1.0f);
                  float oy=(1.0f-std::fabs(x))*(y>=0?1.0f:-1.0f); x=ox; y=oy; }
    float l=std::sqrt(x*x+y*y+z*z); if(l<1e-12f)l=1.0f;
    n[0]=x/l; n[1]=y/l; n[2]=z/l;
}

std::vector<FlatTri> flatten_part_triangles_mat(const BLASManager& blas, const TLASManager& tlas) {
    std::vector<FlatTri> out;
    const auto& recs = tlas.get_draw_records();
    for (const auto& r : recs) {
        const BLASManager::BLASEntry* e = blas.get_entry(r.blas_handle);
        if (!e) continue;
        const float* m = r.transform.m; // row-major 4x4
        auto xf = [&](float3 p) {
            return make_float3(
                m[0]*p.x + m[1]*p.y + m[2]*p.z + m[3],
                m[4]*p.x + m[5]*p.y + m[6]*p.z + m[7],
                m[8]*p.x + m[9]*p.y + m[10]*p.z + m[11]);
        };
        for (size_t i = 0; i < e->triangles.size(); ++i) {
            const Tri& t = e->triangles[i];
            FlatTri f{};
            f.v0 = xf(t.vertex0); f.v1 = xf(t.vertex1); f.v2 = xf(t.vertex2);
            if (i < e->tri_extra.size()) {
                f.materialId = e->tri_extra[i].materialId;
                const float4& tn = e->tri_extra[i].tint;
                f.tint[0] = tn.x; f.tint[1] = tn.y; f.tint[2] = tn.z; f.tint[3] = tn.w;
            } else {
                f.materialId = static_cast<int>(r.material_id);
                f.tint[0] = 1.0f; f.tint[1] = 1.0f; f.tint[2] = 1.0f; f.tint[3] = 0.0f;
            }
            out.push_back(f);
        }
    }
    return out;
}

} // namespace voxel_imposter
