#include <cstdio>
#include <cstdint>
#include <cstring>
extern "C" {
#include "quickjs.h"
}
#include "../include/script_host.h"
#include "../include/dsl_state.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); ++failures; } } while (0)

static void test_embed_eval_1_plus_1() {
    JSRuntime* rt = JS_NewRuntime();
    CHECK(rt != nullptr, "runtime created");
    JSContext* ctx = JS_NewContext(rt);
    CHECK(ctx != nullptr, "context created");
    JSValue v = JS_Eval(ctx, "1+1", 3, "<test>", JS_EVAL_TYPE_GLOBAL);
    CHECK(!JS_IsException(v), "eval did not throw");
    int32_t out = -1;
    CHECK(JS_ToInt32(ctx, &out, v) == 0, "result convertible to int");
    CHECK(out == 2, "1+1 == 2");
    JS_FreeValue(ctx, v);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_fresh_context_runs_empty_class() {
    script_host::ScriptHost host;
    const char* src =
        "class Empty extends Part {\n"
        "  static params = {};\n"
        "  build(p) {}\n"
        "}\n";
    script_host::BakeResult r = host.bake_source(src, "{}", {});
    CHECK(r.error.ok, "empty class bakes without error");
}

static void test_build_called_on_authored_class() {
    script_host::ScriptHost host;
    const char* src =
        "class Rock extends Part {\n"
        "  static params = {};\n"
        "  build(p) { globalThis.__built = 1; }\n"
        "}\n";
    script_host::BakeResult r = host.bake_source(src, "{}", {});
    CHECK(r.error.ok, "authored class bakes");
    CHECK(host.last_build_ran(), "build(p) was invoked");
}

static void test_dsl_state_rules() {
    dsl::DslState s;
    // pop on empty-above-identity is misuse
    s.popMatrix();
    CHECK(s.has_error(), "popMatrix below identity is an error");

    dsl::DslState s2;
    s2.endVoxels();
    CHECK(s2.has_error(), "endVoxels with no open session is an error");

    dsl::DslState s3;
    s3.sphere({0,0,0}, 1.0f, dsl::CsgOp::Union);
    CHECK(s3.has_error(), "emitting outside a session is an error");

    dsl::DslState s4;
    s4.beginVoxels(0.1f);
    s4.beginVoxels(0.1f);
    CHECK(s4.has_error(), "opening a session inside another is an error");

    dsl::DslState s5;
    s5.beginVoxels(0.25f);
    s5.fill(7);
    s5.sphere({1,0,0}, 2.0f, dsl::CsgOp::Union);
    s5.endVoxels();
    CHECK(!s5.has_error(), "valid voxel session has no error");
    CHECK(s5.buffer().ops.size() == 1, "one brush recorded");
    CHECK(s5.buffer().ops[0].materialId == 7, "material cursor captured");
    CHECK(s5.buffer().ops[0].spacing == 0.25f, "session spacing captured");
}

int main() {
    test_embed_eval_1_plus_1();
    test_fresh_context_runs_empty_class();
    test_build_called_on_authored_class();
    test_dsl_state_rules();
    if (failures == 0) printf("ALL PASS\n");
    return failures ? 1 : 0;
}
