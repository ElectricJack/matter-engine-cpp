#include "../include/dsl_state.h"
#define RAYMATH_IMPLEMENTATION
#include "raymath.h"

namespace dsl {

DslState::DslState() { stack_.push_back(MatrixIdentity()); }

void DslState::pushMatrix() { stack_.push_back(stack_.back()); }
void DslState::popMatrix() {
    if (stack_.size() <= 1) { set_error("popMatrix without matching pushMatrix"); return; }
    stack_.pop_back();
}
void DslState::translate(float x, float y, float z) {
    stack_.back() = MatrixMultiply(MatrixTranslate(x,y,z), stack_.back());
}
void DslState::rotateX(float r){ stack_.back()=MatrixMultiply(MatrixRotateX(r),stack_.back()); }
void DslState::rotateY(float r){ stack_.back()=MatrixMultiply(MatrixRotateY(r),stack_.back()); }
void DslState::rotateZ(float r){ stack_.back()=MatrixMultiply(MatrixRotateZ(r),stack_.back()); }
void DslState::scale(float x,float y,float z){ stack_.back()=MatrixMultiply(MatrixScale(x,y,z),stack_.back()); }
void DslState::applyMatrix(const float m[16]) {
    Matrix mm = { m[0],m[1],m[2],m[3], m[4],m[5],m[6],m[7],
                  m[8],m[9],m[10],m[11], m[12],m[13],m[14],m[15] };
    stack_.back() = MatrixMultiply(mm, stack_.back());
}

void DslState::beginVoxels(float spacing) {
    if (session_ != Session::None) { set_error("beginVoxels inside an open session"); return; }
    session_ = Session::Voxels; spacing_ = (spacing > 0 ? spacing : 0.1f);
    session_start_ = buffer_.ops.size();
}
void DslState::endVoxels() {
    if (session_ != Session::Voxels) { set_error("endVoxels with no open voxel session"); return; }
    // Whole-expression smoothing: the spec applies smoothing(k) to the whole
    // session's union, not just the brush emitted while the cursor was set. Stamp
    // the final cursor onto every op emitted in this session so smoothing(k)
    // called anywhere in the build (incl. after the brushes) takes effect.
    for (size_t i = session_start_; i < buffer_.ops.size(); ++i)
        buffer_.ops[i].smoothing = smoothing_;
    session_ = Session::None;
}

void DslState::sphere(const Vector3& c, float r, CsgOp op) {
    if (session_ != Session::Voxels) { set_error("sphere() emitted outside a voxel session"); return; }
    BuildOp o{}; o.kind=BrushKind::Sphere; o.op=op; o.transform=stack_.back();
    o.materialId=material_; o.center=c; o.radius=r; o.smoothing=smoothing_; o.spacing=spacing_;
    buffer_.ops.push_back(o);
}
void DslState::box(const Vector3& c, const Vector3& h, CsgOp op) {
    if (session_ != Session::Voxels) { set_error("box() emitted outside a voxel session"); return; }
    BuildOp o{}; o.kind=BrushKind::Box; o.op=op; o.transform=stack_.back();
    o.materialId=material_; o.center=c; o.halfExtents=h; o.smoothing=smoothing_; o.spacing=spacing_;
    buffer_.ops.push_back(o);
}

// raylib Matrix stores its 16 floats column-major (m0,m4,m8,m12 = first row of
// the math matrix). world_flatten / ChildInstance consume row-major, so transpose
// the storage: translation (m12,m13,m14) lands in out[3],out[7],out[11].
static void matrix_to_row16(const Matrix& mm, float out[16]) {
    out[0]=mm.m0;  out[1]=mm.m4;  out[2]=mm.m8;  out[3]=mm.m12;
    out[4]=mm.m1;  out[5]=mm.m5;  out[6]=mm.m9;  out[7]=mm.m13;
    out[8]=mm.m2;  out[9]=mm.m6;  out[10]=mm.m10; out[11]=mm.m14;
    out[12]=mm.m3; out[13]=mm.m7; out[14]=mm.m11; out[15]=mm.m15;
}

void DslState::placeChild(const std::string& module) {
    auto it = child_hashes_.find(module);
    if (it == child_hashes_.end()) {
        set_error("placeChild: undeclared child '" + module +
                  "' (add it to static requires)");
        return;
    }
    ChildPlacement p;
    p.hash = it->second;
    matrix_to_row16(top(), p.transform);
    children_.push_back(p);
}

} // namespace dsl
