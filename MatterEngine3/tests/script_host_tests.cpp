#include <cstdio>
#include <cstdint>
#include <cstring>
extern "C" {
#include "quickjs.h"
}
#include "../include/script_host.h"

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

int main() {
    test_embed_eval_1_plus_1();
    test_fresh_context_runs_empty_class();
    test_build_called_on_authored_class();
    if (failures == 0) printf("ALL PASS\n");
    return failures ? 1 : 0;
}
