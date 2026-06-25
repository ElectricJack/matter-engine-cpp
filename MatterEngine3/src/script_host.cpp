#include "../include/script_host.h"
extern "C" {
#include "quickjs.h"
}
#include "part_base.js.h"
#include <cstring>
#include <regex>

namespace script_host {

// Extract the authored class name from `class <Name> extends Part`. Top-level
// `class` declarations in GLOBAL eval create LEXICAL bindings (not enumerable
// globalThis properties), so the host cannot discover them by scanning the
// global object. Instead the host appends a trampoline that assigns the named
// class to globalThis.__partClass; this is generic over the class name (no
// hardcoded "Empty"/"Rock") and deterministic.
static std::string find_part_class_name(const std::string& source) {
    std::smatch m;
    std::regex re("class\\s+([A-Za-z_$][A-Za-z0-9_$]*)\\s+extends\\s+Part\\b");
    if (std::regex_search(source, m, re)) return m[1].str();
    return std::string();
}

// Pulls the current exception into a BakeError (best-effort location).
static BakeError harvest_exception(JSContext* ctx) {
    BakeError e; e.ok = false;
    JSValue ex = JS_GetException(ctx);
    const char* msg = JS_ToCString(ctx, ex);
    e.message = msg ? msg : "unknown script error";
    if (msg) JS_FreeCString(ctx, msg);
    JSValue stack = JS_GetPropertyStr(ctx, ex, "stack");
    if (!JS_IsUndefined(stack)) {
        const char* s = JS_ToCString(ctx, stack);
        if (s) { e.source_location = s; JS_FreeCString(ctx, s); }
    }
    JS_FreeValue(ctx, stack);
    JS_FreeValue(ctx, ex);
    return e;
}

BakeResult ScriptHost::bake_source(const std::string& source,
                                   const std::string& /*params_json*/,
                                   const BakeOptions& /*opts*/,
                                   const uint64_t* /*child_hashes*/,
                                   size_t /*child_count*/) {
    BakeResult r;
    JSRuntime* rt = JS_NewRuntime();
    JSContext* ctx = JS_NewContext(rt);

    last_build_ran_ = false;
    JSValue base = JS_Eval(ctx, kPartBaseJS, strlen(kPartBaseJS), "<part-base>",
                           JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(base)) { r.error = harvest_exception(ctx); JS_FreeValue(ctx,base); goto done; }
    JS_FreeValue(ctx, base);

    {
        // Eval user source + a generic trampoline that publishes the authored
        // class (lexically declared) onto globalThis.__partClass.
        std::string className = find_part_class_name(source);
        if (className.empty()) {
            r.error.ok = false; r.error.message = "no class extending Part found";
            goto done;
        }
        std::string wrapped = source + "\n;globalThis.__partClass = " + className + ";\n";
        JSValue v = JS_Eval(ctx, wrapped.c_str(), wrapped.size(), "<part>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(v)) { r.error = harvest_exception(ctx); JS_FreeValue(ctx,v); goto done; }
        JS_FreeValue(ctx, v);

        JSValue global = JS_GetGlobalObject(ctx);
        JSValue authored = JS_GetPropertyStr(ctx, global, "__partClass");
        JS_FreeValue(ctx, global);
        if (!JS_IsFunction(ctx, authored)) {
            JS_FreeValue(ctx, authored);
            r.error.ok = false; r.error.message = "no class extending Part found";
            goto done;
        }
        JSValue inst = JS_CallConstructor(ctx, authored, 0, nullptr);
        JS_FreeValue(ctx, authored);
        if (JS_IsException(inst)) { r.error = harvest_exception(ctx); JS_FreeValue(ctx,inst); goto done; }
        JSValue paramsObj = JS_NewObject(ctx);  // merged params arrive in Task 5
        JSAtom buildAtom = JS_NewAtom(ctx, "build");
        JSValue bret = JS_Invoke(ctx, inst, buildAtom, 1, &paramsObj);
        JS_FreeAtom(ctx, buildAtom);
        last_build_ran_ = !JS_IsException(bret);
        if (JS_IsException(bret)) r.error = harvest_exception(ctx);
        JS_FreeValue(ctx, bret);
        JS_FreeValue(ctx, paramsObj);
        JS_FreeValue(ctx, inst);
    }

done:
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return r;
}

} // namespace script_host
