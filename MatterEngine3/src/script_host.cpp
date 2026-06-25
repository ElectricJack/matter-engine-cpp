#include "../include/script_host.h"
extern "C" {
#include "quickjs.h"
}
#include "part_base.js.h"
#include "part_asset_v2.h"   // SP-1 v2 helper (compute_resolved_hash, save_v2)
#include "../include/dsl_state.h"
#include "../include/dsl_bindings.h"
#include "../include/csg_lowering.h"   // NEW MatterEngine3 header
#include "cluster.h"                    // consumed prototype (StaticParticle, Cluster)
#include "cell.h"                       // consumed prototype (Cell, build_cell_meshes GL-free)
#include "mesh_worker_pool.h"           // consumed prototype (CellMeshResult/GroupMeshResult)
#include "blas_manager.hpp"             // consumed prototype
#include "tlas_manager.hpp"             // consumed prototype
#include "surface.h"                    // consumed prototype (CreateSurfaceScratch; self-guards extern "C")
#include <cstring>
#include <cmath>
#include <map>
#include <memory>
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

// Merge static params with caller overrides (overrides win), sort keys, and
// stringify to canonical JSON. Evals the class's `static params` but does NOT
// instantiate or call build(). Shared by bake_source and resolve_hash so both
// hash byte-identical params.
std::string ScriptHost::merge_params_canonical(const std::string& source,
                                               const std::string& params_json,
                                               BakeError& err) {
    last_merged_params_ = "{}";
    std::string className = find_part_class_name(source);
    if (className.empty()) {
        err.ok = false; err.message = "no class extending Part found";
        return last_merged_params_;
    }

    JSRuntime* rt = JS_NewRuntime();
    JSContext* ctx = JS_NewContext(rt);

    JSValue base = JS_Eval(ctx, kPartBaseJS, strlen(kPartBaseJS), "<part-base>",
                           JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(base)) { err = harvest_exception(ctx); JS_FreeValue(ctx,base);
        JS_FreeContext(ctx); JS_FreeRuntime(rt); return last_merged_params_; }
    JS_FreeValue(ctx, base);

    std::string wrapped = source + "\n;globalThis.__partClass = " + className + ";\n";
    JSValue v = JS_Eval(ctx, wrapped.c_str(), wrapped.size(), "<part>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v)) { err = harvest_exception(ctx); JS_FreeValue(ctx,v);
        JS_FreeContext(ctx); JS_FreeRuntime(rt); return last_merged_params_; }
    JS_FreeValue(ctx, v);

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue authored = JS_GetPropertyStr(ctx, global, "__partClass");
    JS_FreeValue(ctx, global);
    if (!JS_IsFunction(ctx, authored)) {
        JS_FreeValue(ctx, authored);
        err.ok = false; err.message = "no class extending Part found";
        JS_FreeContext(ctx); JS_FreeRuntime(rt); return last_merged_params_;
    }

    JSValue staticParams = JS_GetPropertyStr(ctx, authored, "params");
    if (JS_IsUndefined(staticParams)) staticParams = JS_NewObject(ctx);
    JSValue overrides = JS_ParseJSON(ctx, params_json.c_str(), params_json.size(), "<params>");
    if (JS_IsException(overrides)) {
        err = harvest_exception(ctx);
        JS_FreeValue(ctx, staticParams); JS_FreeValue(ctx, overrides);
        JS_FreeValue(ctx, authored);
        JS_FreeContext(ctx); JS_FreeRuntime(rt); return last_merged_params_;
    }
    static const char* kMerge =
      "(function(d,o){let m=Object.assign({},d,o);"
      "let keys=Object.keys(m).sort();let r={};for(let k of keys)r[k]=m[k];"
      "return JSON.stringify(r);})";
    JSValue mergeFn = JS_Eval(ctx, kMerge, strlen(kMerge), "<merge>", JS_EVAL_TYPE_GLOBAL);
    JSValue args2[2] = { staticParams, overrides };
    JSValue mergedStr = JS_Call(ctx, mergeFn, JS_UNDEFINED, 2, args2);
    if (JS_IsException(mergedStr)) {
        err = harvest_exception(ctx);
    } else {
        const char* mjson = JS_ToCString(ctx, mergedStr);
        last_merged_params_ = mjson ? mjson : "{}";
        if (mjson) JS_FreeCString(ctx, mjson);
    }
    JS_FreeValue(ctx, mergedStr); JS_FreeValue(ctx, mergeFn);
    JS_FreeValue(ctx, overrides); JS_FreeValue(ctx, staticParams);
    JS_FreeValue(ctx, authored);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return last_merged_params_;
}

uint64_t ScriptHost::resolve_hash(const std::string& source,
                                  const std::string& params_json,
                                  const uint64_t* child_hashes,
                                  size_t child_count) {
    BakeError err;
    std::string canon = merge_params_canonical(source, params_json, err);
    if (!err.ok) return 0;   // fail-closed: caller treats 0 as resolve failure
    return part_asset::compute_resolved_hash(
        source.data(), source.size(),
        canon.data(), canon.size(),
        child_hashes, child_count);
}

BakeResult ScriptHost::bake_source(const std::string& source,
                                   const std::string& params_json,
                                   const BakeOptions& /*opts*/,
                                   const uint64_t* child_hashes,
                                   size_t child_count) {
    BakeResult r;

    // Merge static params + caller overrides into canonical JSON, and compute
    // the resolved hash over (source, merged params, child hashes). Shares the
    // exact path resolve_hash uses so both agree byte-for-byte.
    {
        BakeError merr;
        std::string canon = merge_params_canonical(source, params_json, merr);
        if (!merr.ok) { r.error = merr; return r; }
        r.resolved_hash = part_asset::compute_resolved_hash(
            source.data(), source.size(),
            canon.data(), canon.size(),
            child_hashes, child_count);
    }
    const std::string merged = last_merged_params_;

    JSRuntime* rt = JS_NewRuntime();
    JSContext* ctx = JS_NewContext(rt);

    // C++-owned authoring state for this bake; native DSL bindings mutate it.
    dsl::DslState state;
    JS_SetContextOpaque(ctx, &state);

    last_build_ran_ = false;
    last_buffer_.clear();
    {
    JSValue base = JS_Eval(ctx, kPartBaseJS, strlen(kPartBaseJS), "<part-base>",
                           JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(base)) { r.error = harvest_exception(ctx); JS_FreeValue(ctx,base); goto done; }
    JS_FreeValue(ctx, base);
    dsl::install_bindings(ctx);

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
        JSValue paramsObj = JS_ParseJSON(ctx, merged.c_str(), merged.size(), "<merged>");
        if (JS_IsException(paramsObj)) { JS_FreeValue(ctx, paramsObj); paramsObj = JS_NewObject(ctx); }
        JSAtom buildAtom = JS_NewAtom(ctx, "build");
        JSValue bret = JS_Invoke(ctx, inst, buildAtom, 1, &paramsObj);
        JS_FreeAtom(ctx, buildAtom);
        last_build_ran_ = !JS_IsException(bret);
        if (JS_IsException(bret)) r.error = harvest_exception(ctx);
        JS_FreeValue(ctx, bret);
        JS_FreeValue(ctx, paramsObj);
        JS_FreeValue(ctx, inst);
    }
    } // close DslState-scope block

    // Fail-closed: a DSL session/transform misuse during build surfaces here.
    last_buffer_ = state.buffer();
    if (r.error.ok && state.has_error()) {
        r.error.ok = false;
        r.error.message = state.error();
    }
    // A session left open at end of build is a misuse.
    if (r.error.ok && state.session() != dsl::Session::None) {
        r.error.ok = false;
        r.error.message = "session left open at end of build";
    }

    // Success path: lower the build buffer to particles, surface per-cell BLAS,
    // and serialize one .part via SP-1's save_v2. Fail-closed: writes nothing on
    // any error branch above (r.error.ok already false there).
    //
    // The prototype's Cluster::force_rebuild_all_cells commits each cell mesh via
    // Cell::commit_cell_meshes, which calls raylib UploadMesh (a GL upload). The
    // bake is headless (no GL context), so we cannot drive Cluster. Instead we
    // partition particles into Cells exactly as the prototype does, mesh each cell
    // GL-FREE via Cell::build_cell_meshes, and register the resulting triangle
    // arrays straight into the BLAS (the same register_triangles path the headless
    // part-asset tests use), skipping the GL UploadMesh entirely.
    if (r.error.ok) {
        dsl::LoweredField f = dsl::lower_build_buffer(state.buffer());
        const float cell_size = 1.0f;   // smallest_cell_size (matches Cluster default)
        const float base_detail = state.buffer().ops.empty()
                                      ? 0.1f : state.buffer().ops[0].spacing;

        BLASManager blas; TLASManager tlas;

        // Build the additive particle vector once (Cell reads it by index).
        const std::vector<StaticParticle>& particles = f.additive;

        // Determine the set of integer cell coordinates touched by any additive
        // particle, using the prototype's influence_radius = radius * 2 halo.
        std::map<std::tuple<int,int,int>, std::unique_ptr<Cell>> cells;
        auto cell_key = [](int x,int y,int z){ return std::make_tuple(x,y,z); };
        for (const StaticParticle& sp : particles) {
            float inf = sp.radius * 2.0f;
            int x0 = (int)std::floor((sp.position.x - inf) / cell_size);
            int x1 = (int)std::floor((sp.position.x + inf) / cell_size);
            int y0 = (int)std::floor((sp.position.y - inf) / cell_size);
            int y1 = (int)std::floor((sp.position.y + inf) / cell_size);
            int z0 = (int)std::floor((sp.position.z - inf) / cell_size);
            int z1 = (int)std::floor((sp.position.z + inf) / cell_size);
            for (int x=x0;x<=x1;++x) for (int y=y0;y<=y1;++y) for (int z=z0;z<=z1;++z) {
                auto k = cell_key(x,y,z);
                if (cells.find(k) == cells.end())
                    cells[k] = std::make_unique<Cell>(Vector3{(float)x,(float)y,(float)z},
                                                      0, cell_size);
            }
        }

        SurfaceScratch* scratch = CreateSurfaceScratch();
        for (auto& kv : cells) {
            Cell* cell = kv.second.get();
            // Assign overlapping additive particles to this cell, grouped by material.
            cell->clear_particle_indices();
            for (uint32_t i = 0; i < particles.size(); ++i) {
                const StaticParticle& sp = particles[i];
                if (cell->intersects_sphere(sp.position, sp.radius))
                    cell->add_particle_index(i, sp.materialId);
            }
            if (cell->material_particle_indices.empty()) continue;

            // Gather carve particles whose influence overlaps this cell (mirrors
            // the prototype's intersects_sphere halo with the same 1.5x slack).
            std::vector<Particle> carve;
            for (const Particle& cp : f.carve)
                if (cell->intersects_sphere(cp.position, cp.radius * 1.5f))
                    carve.push_back(cp);
            const Particle* carvePtr = carve.empty() ? nullptr : carve.data();
            int carveCount = (int)carve.size();

            CellMeshResult res = cell->build_cell_meshes(
                particles, scratch, /*simplification*/1.0f, base_detail,
                /*max_pow*/6, /*uniform_detail*/0.0f, carvePtr, carveCount);

            // Register each group's GL-free triangle arrays directly into the BLAS
            // and place an identity instance in the TLAS.
            for (GroupMeshResult& g : res.groups) {
                if (g.triangles.empty()) continue;
                BLASHandle h = blas.register_triangles(g.triangles, g.triangle_normals);
                if (h != INVALID_BLAS_HANDLE) {
                    tlas.load_identity();
                    tlas.draw(h, g.group_id);
                }
            }
        }
        DestroySurfaceScratch(scratch);

        tlas.build(blas);
        std::string path = part_asset::cache_path_resolved(r.resolved_hash);
        part_asset::LodLevels lods{};   // SP-2 writes no children, empty LOD array.
        bool ok = part_asset::save_v2(path, blas, tlas, /*children*/nullptr, 0,
                                      lods, r.resolved_hash);
        if (!ok) { r.error.ok = false; r.error.message = "save_v2 failed"; }
        else { r.written_path = path; }
    }

done:
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return r;
}

} // namespace script_host
