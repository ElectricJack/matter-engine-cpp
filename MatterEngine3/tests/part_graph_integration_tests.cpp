// SP-3 Task 12 end-to-end check (gated on SP-2 via -DMATTER_HAVE_SCRIPT_HOST).
//
// Writes real .js part schemas + a world manifest, runs PartGraph::install
// through the REAL FileModuleResolver + HostBaker adapters (which drive the SP-2
// ScriptHost: eval_requires for child discovery, resolve_hash for cache keys,
// bake_source -> save_v2 for the actual .part files), then asserts the expected
// parts/<hash>.part files exist on disk and a second install bakes 0 (all hits).
//
// bake_source writes to the RELATIVE path cache_path_resolved() = "parts/<hash>.part",
// so this test chdir()s into a fresh temp dir and points HostBaker at "parts" so
// both the writer and the cache check agree on the location (plan precondition).

#include "part_graph.h"        // includes script_host.h under the guard
#include "part_asset_v2.h"     // cache_path_resolved
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); ++failures; } } while (0)

static bool file_exists(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0; }

static void write_file(const std::string& path, const std::string& body) {
    std::ofstream f(path, std::ios::binary);
    f << body;
}

int main() {
    using namespace part_graph;

    // Fresh sandbox so parts/<hash>.part and the schemas live in a known place.
    const std::string root = "/tmp/me3_graph_integration";
    system(("rm -rf " + root).c_str());
    const std::string schemas = root + "/schemas";
    const std::string parts   = root + "/parts";   // == <root>/parts; we chdir to <root>
    system(("mkdir -p " + schemas + " " + parts).c_str());

    // A two-part graph: Wall (leaf, no children) and Tower (parent that requires
    // two Wall instances with identical params -> dedup to ONE Wall artifact).
    write_file(schemas + "/Wall.js",
        "class Wall extends Part {\n"
        "  static params = { h: 1.0 };\n"
        "  build(p) { this.beginVoxels(0.25); this.fill(MAT.stone);\n"
        "             this.box([0,0,0],[0.5,p.h,0.5]); this.endVoxels(); }\n"
        "}\n");
    write_file(schemas + "/Tower.js",
        "class Tower extends Part {\n"
        "  static params = {};\n"
        "  static requires(p) {\n"
        "    return [ { module: 'Wall', params: { h: 1.0 } },\n"
        "             { module: 'Wall', params: { h: 1.0 } } ];\n"
        "  }\n"
        "  build(p) { this.beginVoxels(0.25); this.fill(MAT.stone);\n"
        "             this.box([0,0,0],[0.5,2.0,0.5]); this.endVoxels(); }\n"
        "}\n");

    // chdir so bake_source's relative "parts/<hash>.part" lands in <root>/parts.
    char prevcwd[4096]; if (!getcwd(prevcwd, sizeof prevcwd)) prevcwd[0] = '\0';
    CHECK(chdir(root.c_str()) == 0, "chdir into sandbox");

    script_host::ScriptHost host;
    FileModuleResolver resolver(host, "schemas");
    // cache_path_resolved() already yields "parts/<hash>.part" (relative to cwd),
    // and bake_source writes there; HostBaker::cached joins parts_dir_ + "/" +
    // cache_path_resolved(), so parts_dir_ is the PARENT of parts/ (here, ".").
    HostBaker baker(host, ".");
    PartGraph graph(resolver, baker);

    // First install: Tower + (deduped) Wall => 2 artifacts baked, 0 hits.
    InstallResult r1 = graph.install({ ChildRequest{"Tower", Params{}} });
    CHECK(r1.ok, "first install succeeds end-to-end");
    if (!r1.ok) printf("  install error: %s\n", r1.error.c_str());
    CHECK(r1.baked.size() == 2, "first install bakes exactly 2 artifacts (Tower + 1 deduped Wall)");
    CHECK(r1.hits == 0, "first install has no cache hits");

    // The .part files for each baked hash must now be on disk.
    for (uint64_t h : r1.baked) {
        std::string path = part_asset::cache_path_resolved(h); // "parts/<hash>.part"
        CHECK(file_exists(path), "baked .part exists on disk");
    }

    // Second install: everything is cached now => 0 bakes, all hits.
    InstallResult r2 = graph.install({ ChildRequest{"Tower", Params{}} });
    CHECK(r2.ok, "second install succeeds");
    CHECK(r2.baked.empty(), "second install bakes nothing (incremental cache hit)");
    CHECK(r2.hits == (int)r1.baked.size(), "second install reports a hit for every prior artifact");

    if (prevcwd[0]) (void)chdir(prevcwd);
    system(("rm -rf " + root).c_str());

    if (failures == 0) printf("All part_graph integration tests passed\n");
    return failures == 0 ? 0 : 1;
}
