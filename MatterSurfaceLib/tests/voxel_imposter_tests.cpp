#include "../include/voxel_imposter.h"
#include <cstdio>
#include <cmath>
using namespace voxel_imposter;
static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); ++failures; } } while (0)

static void test_grid_dims_cube() {
    float lo[3]={0,0,0}, hi[3]={2,2,2}; int nx,ny,nz;
    CHECK(choose_grid_dims(lo,hi,128,nx,ny,nz), "cube dims ok");
    CHECK(nx==128 && ny==128 && nz==128, "cube -> 128^3");
}
static void test_grid_dims_flat() {
    float lo[3]={0,0,0}, hi[3]={4,4,1}; int nx,ny,nz; // z is 1/4 the extent
    CHECK(choose_grid_dims(lo,hi,128,nx,ny,nz), "flat dims ok");
    CHECK(nx==128 && ny==128 && nz==32, "flat brick -> 128x128x32");
}
static void test_grid_dims_degenerate() {
    float lo[3]={0,0,0}, hi[3]={0,0,0}; int nx,ny,nz;
    CHECK(!choose_grid_dims(lo,hi,128,nx,ny,nz), "zero extent rejected");
}

static void test_tribox_hit() {
    float c[3]={0,0,0}, h[3]={1,1,1};
    float a[3]={-2,0,0}, b[3]={2,0,0}, d[3]={0,2,0}; // big tri through the box
    CHECK(tri_box_overlap(c,h,a,b,d), "triangle crossing box overlaps");
}
static void test_tribox_miss() {
    float c[3]={0,0,0}, h[3]={1,1,1};
    float a[3]={5,5,5}, b[3]={6,5,5}, d[3]={5,6,5}; // far away
    CHECK(!tri_box_overlap(c,h,a,b,d), "distant triangle does not overlap");
}

static void test_oct_roundtrip() {
    const float ns[][3] = {{0,0,1},{0,0,-1},{1,0,0},{0,1,0},
                           {0.577f,0.577f,0.577f},{-0.5f,0.7f,-0.5f}};
    for (auto& n : ns) {
        float ln=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        float u[3]={n[0]/ln,n[1]/ln,n[2]/ln};
        uint8_t enc[2]; oct_encode(u,enc);
        float dec[3]; oct_decode(enc,dec);
        float err=std::fabs(u[0]-dec[0])+std::fabs(u[1]-dec[1])+std::fabs(u[2]-dec[2]);
        CHECK(err<0.02f, "oct round-trip within tolerance");
    }
}

int main(){
    test_grid_dims_cube(); test_grid_dims_flat(); test_grid_dims_degenerate();
    test_tribox_hit(); test_tribox_miss();
    test_oct_roundtrip();
    if(!failures) printf("All voxel_imposter tests passed\n");
    return failures?1:0;
}
