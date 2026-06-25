#include "../include/dsl_state.h"
#include "../include/dsl_bindings.h"
extern "C" {
#include "quickjs.h"
}

namespace dsl {

static DslState* state_of(JSContext* ctx) {
    return static_cast<DslState*>(JS_GetContextOpaque(ctx));
}
static double argd(JSContext* ctx, JSValueConst v) { double d=0; JS_ToFloat64(ctx,&d,v); return d; }

static JSValue j_pushMatrix(JSContext* c, JSValueConst, int, JSValueConst*) { state_of(c)->pushMatrix(); return JS_UNDEFINED; }
static JSValue j_popMatrix (JSContext* c, JSValueConst, int, JSValueConst*) { state_of(c)->popMatrix();  return JS_UNDEFINED; }
static JSValue j_translate(JSContext* c, JSValueConst, int n, JSValueConst* a){ state_of(c)->translate(argd(c,a[0]),argd(c,a[1]),argd(c,a[2])); return JS_UNDEFINED; }
static JSValue j_rotateX(JSContext* c, JSValueConst, int, JSValueConst* a){ state_of(c)->rotateX(argd(c,a[0])); return JS_UNDEFINED; }
static JSValue j_rotateY(JSContext* c, JSValueConst, int, JSValueConst* a){ state_of(c)->rotateY(argd(c,a[0])); return JS_UNDEFINED; }
static JSValue j_rotateZ(JSContext* c, JSValueConst, int, JSValueConst* a){ state_of(c)->rotateZ(argd(c,a[0])); return JS_UNDEFINED; }
static JSValue j_scale(JSContext* c, JSValueConst, int, JSValueConst* a){ state_of(c)->scale(argd(c,a[0]),argd(c,a[1]),argd(c,a[2])); return JS_UNDEFINED; }
static JSValue j_applyMatrix(JSContext* c, JSValueConst, int, JSValueConst* a){
    float m[16]; for (int i=0;i<16;++i){ JSValue e=JS_GetPropertyUint32(c,a[0],i); m[i]=(float)argd(c,e); JS_FreeValue(c,e);} state_of(c)->applyMatrix(m); return JS_UNDEFINED; }
static JSValue j_fill(JSContext* c, JSValueConst, int, JSValueConst* a){ int32_t id=0; JS_ToInt32(c,&id,a[0]); state_of(c)->fill((uint32_t)id); return JS_UNDEFINED; }
static JSValue j_beginVoxels(JSContext* c, JSValueConst, int, JSValueConst* a){ state_of(c)->beginVoxels((float)argd(c,a[0])); return JS_UNDEFINED; }
static JSValue j_endVoxels(JSContext* c, JSValueConst, int, JSValueConst*){ state_of(c)->endVoxels(); return JS_UNDEFINED; }
static JSValue j_sphere(JSContext* c, JSValueConst, int, JSValueConst* a){
    state_of(c)->sphere({(float)argd(c,a[0]),(float)argd(c,a[1]),(float)argd(c,a[2])},(float)argd(c,a[3]),CsgOp::Union); return JS_UNDEFINED; }
static JSValue j_box(JSContext* c, JSValueConst, int, JSValueConst* a){
    state_of(c)->box({(float)argd(c,a[0]),(float)argd(c,a[1]),(float)argd(c,a[2])},
                     {(float)argd(c,a[3]),(float)argd(c,a[4]),(float)argd(c,a[5])},CsgOp::Union); return JS_UNDEFINED; }
static JSValue j_op(JSContext* c, JSValueConst, int, JSValueConst* a){
    int32_t k=0; JS_ToInt32(c,&k,a[0]); state_of(c)->set_last_op((CsgOp)k); return JS_UNDEFINED; }
static JSValue j_smoothing(JSContext* c, JSValueConst, int, JSValueConst* a){ state_of(c)->smoothing((float)argd(c,a[0])); return JS_UNDEFINED; }

void install_bindings(JSContext* ctx) {
    JSValue g = JS_GetGlobalObject(ctx);
    auto bind=[&](const char* n, JSCFunction* f, int argc){ JS_SetPropertyStr(ctx,g,n,JS_NewCFunction(ctx,f,n,argc)); };
    bind("__dsl_pushMatrix",j_pushMatrix,0); bind("__dsl_popMatrix",j_popMatrix,0);
    bind("__dsl_translate",j_translate,3);
    bind("__dsl_rotateX",j_rotateX,1); bind("__dsl_rotateY",j_rotateY,1); bind("__dsl_rotateZ",j_rotateZ,1);
    bind("__dsl_scale",j_scale,3); bind("__dsl_applyMatrix",j_applyMatrix,1);
    bind("__dsl_fill",j_fill,1);
    bind("__dsl_beginVoxels",j_beginVoxels,1); bind("__dsl_endVoxels",j_endVoxels,0);
    bind("__dsl_sphere",j_sphere,4); bind("__dsl_box",j_box,6);
    bind("__dsl_op",j_op,1); bind("__dsl_smoothing",j_smoothing,1);
    JS_FreeValue(ctx,g);
}

} // namespace dsl
