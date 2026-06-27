// Headless tests for the GL-free viewer units (WorldState, resolvers, PartStore,
// LocalProvider cache behavior). Run via `make run-viewer-logic`.
#include "../viewer/world_source.h"
#include "../viewer/sector_resolver.h"
#include "../viewer/part_store.h"
#include "../viewer/local_provider.h"
#include "../viewer/world_composer.h"
#include "lod_select.h"   // PartLodTable, PartLod
#include "part_graph.h"   // PartGraph + FileModuleResolver/HostBaker (script-host guarded)
#include "part_asset_v2.h"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

static int g_failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", (msg)); ++g_failures; } \
    else        { printf("ok:   %s\n", (msg)); } } while (0)

static viewer::WorldManifestEntry mk_entry(uint32_t id, uint64_t hash, float x) {
    viewer::WorldManifestEntry e{};
    e.instance_id = id;
    e.part_hash   = hash;
    e.transform[0] = e.transform[5] = e.transform[10] = e.transform[15] = 1.0f;
    e.transform[3] = x;   // translate-x
    return e;
}

static void test_world_state_delta() {
    viewer::WorldState state;
    viewer::WorldManifest m;
    m.world_root_hash = 1;
    m.instances.push_back(mk_entry(10, 0xAAAA, 1.0f));
    m.instances.push_back(mk_entry(11, 0xBBBB, 2.0f));
    state.reset(m);
    CHECK(state.entries().size() == 2, "reset loads manifest entries");

    viewer::WorldDelta d;
    d.added.push_back(mk_entry(12, 0xCCCC, 3.0f));   // new
    d.added.push_back(mk_entry(10, 0xAAAA, 9.0f));   // move existing id 10
    d.removed.push_back(11);                          // drop id 11
    state.apply(d);

    CHECK(state.entries().size() == 2, "delta: add one, remove one -> net 2");
    const viewer::WorldManifestEntry* moved = state.find(10);
    CHECK(moved && moved->transform[3] == 9.0f, "delta: id 10 transform updated in place");
    CHECK(state.find(11) == nullptr, "delta: id 11 removed");
    CHECK(state.find(12) != nullptr, "delta: id 12 added");
}

static void test_resolvers() {
    const uint64_t kPart = 0xF00DULL;
    // Two instances of one part, far apart on the x axis.
    viewer::WorldState state;
    viewer::WorldManifest m; m.world_root_hash = 1;
    m.instances.push_back(mk_entry(1, kPart, 0.0f));
    m.instances.push_back(mk_entry(2, kPart, 200.0f));
    state.reset(m);

    // LOD table: one part, bound radius 1.0, three thresholds (coarser = larger).
    lod_select::PartLodTable lods;
    lods[kPart] = lod_select::PartLod{ 1.0f, { 0.50f, 0.20f, 0.05f } };

    // PassThrough: everything active at LOD 0, ignores camera.
    viewer::PassThroughResolver pass;
    auto a = pass.resolve(state, lods, make_float3(0,0,0));
    CHECK(a.size() == 2, "passthrough activates all instances");
    CHECK(a[0].lod_level == 0 && a[1].lod_level == 0, "passthrough uses LOD 0");

    // SectorLod with a large activation radius so BOTH instances stay active and
    // the test exercises LOD selection (not culling): near camera keeps the near
    // instance fine, far camera coarsens.
    viewer::SectorLodResolver sec(16.0f /*pitch*/, 1000.0f /*active radius*/);
    auto near = sec.resolve(state, lods, make_float3(0,4,-4));
    bool near_present = false; int near_lod = -1;
    for (auto& r : near) if (r.transform[3] == 0.0f) { near_present = true; near_lod = r.lod_level; }
    CHECK(near_present, "sectorlod keeps the near instance active");
    CHECK(near_lod >= 0, "sectorlod assigned the near instance a valid LOD");

    // Far camera: within activation radius but farther from instances -> coarser-or-equal LOD.
    // (0,4000,-4000) would be outside the 1000-unit sphere; use (100,200,-200) which is
    // ~295 units from the origin sector and ~300 from the near instance.
    auto far_view = sec.resolve(state, lods, make_float3(100, 200, -200));
    int far_max_lod = 0;
    for (auto& r : far_view) far_max_lod = (r.lod_level > far_max_lod) ? r.lod_level : far_max_lod;
    CHECK(far_max_lod >= near_lod, "sectorlod picks coarser-or-equal LOD from far camera");
}

static void test_part_store_missing() {
    viewer::PartStore store("/tmp/me3_viewer_test_cache_empty");
    CHECK(!store.has(0xDEADBEEFULL), "fresh store reports unknown hash as absent");
    CHECK(store.loaded_count() == 0, "fresh store has nothing loaded");
}

static void test_local_provider_cache() {
    const std::string cache = "/tmp/me3_viewer_cache_test";
    system(("rm -rf " + cache).c_str());

    // Resolve committed example assets relative to MatterEngine3/tests (cwd).
    viewer::LocalProviderConfig cfg;
    cfg.schemas_dir    = "../examples/world_demo/schemas";
    cfg.world_data_dir = "../examples/world_demo/WorldData";
    cfg.world_name     = "Demo";
    cfg.shared_lib_dir = "../shared-lib";
    cfg.cache_root     = cache;

    // --- Cold run: bake everything, want everything. ---
    {
        viewer::LocalProvider prov(cfg);
        viewer::WorldManifest m; std::string err;
        CHECK(prov.connect(m, err), "cold connect succeeds");
        CHECK(!m.instances.empty(), "cold connect yields placed instances");
        CHECK(prov.baked_count() > 0, "cold connect bakes parts (cache miss)");

        viewer::PartStore store(cache);
        auto want = prov.reconcile(m, store);
        CHECK(!want.empty(), "cold reconcile wants the missing parts");
        CHECK(prov.fetch_parts(want, store, err), "cold fetch_parts loads wanted parts");
        CHECK(store.loaded_count() == want.size(), "cold fetch loads every wanted hash");
    }

    // --- Warm run: same cache, nothing changed -> bake nothing, want nothing. ---
    {
        viewer::LocalProvider prov(cfg);
        viewer::WorldManifest m; std::string err;
        CHECK(prov.connect(m, err), "warm connect succeeds");
        CHECK(prov.baked_count() == 0, "warm connect bakes nothing (all cache hits)");

        viewer::PartStore store(cache);
        auto want = prov.reconcile(m, store);
        CHECK(want.empty(), "warm reconcile wants nothing (instant reload)");
    }
}

static void test_composer_counts() {
    const std::string cache = "/tmp/me3_viewer_cache_test";  // reuse Task 4's warm cache
    viewer::LocalProviderConfig cfg;
    cfg.schemas_dir = "../examples/world_demo/schemas";
    cfg.world_data_dir = "../examples/world_demo/WorldData";
    cfg.world_name = "Demo";
    cfg.shared_lib_dir = "../shared-lib";
    cfg.cache_root = cache;

    viewer::LocalProvider prov(cfg);
    viewer::WorldManifest m; std::string err;
    if (!prov.connect(m, err)) { CHECK(false, "composer test: connect"); return; }
    viewer::PartStore store(cache);
    auto want = prov.reconcile(m, store);
    prov.fetch_parts(want, store, err);

    viewer::WorldState state; state.reset(m);

    viewer::WorldComposer composer(store, m.instances.size() + 16);
    auto lods = store.part_lod_table();

    viewer::PassThroughResolver pass;
    int active_all = composer.compose(state, pass, lods, make_float3(0,0,0));
    CHECK(active_all == (int)m.instances.size(), "passthrough composes every instance");

    // Far camera with a small activation radius -> fewer active instances.
    viewer::SectorLodResolver sec(16.0f, 32.0f);
    int active_far = composer.compose(state, sec, lods, make_float3(1000,1000,1000));
    CHECK(active_far < active_all, "sectorlod from far/small-radius composes fewer");
}

static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss; ss << f.rdbuf();
    return ss.str();
}

// Task 10: the PartStore must KEEP the baked child-instance table on its LoadedPart
// (the next task's WorldComposer expands it into the TLAS). We install the REAL demo
// Tree through the same part_graph + ScriptHost harness the integration test uses
// (which bakes parts/<hash>.part relative to cwd), then point a PartStore at that
// sandbox and assert get_or_load(tree)->children is non-empty (the placed Leaf rows).
static void test_partstore_keeps_children() {
    namespace pg = part_graph;

    // Resolve the demo schemas + shared-lib to ABSOLUTE paths BEFORE we chdir, so
    // they still resolve from inside the sandbox (they are repo-relative to tests/).
    std::string schemas, sharedlib;
    { char abs[4096];
      if (realpath("../examples/world_demo/schemas", abs)) schemas = abs;
      if (realpath("../shared-lib", abs)) sharedlib = abs; }
    CHECK(!schemas.empty() && !sharedlib.empty(),
          "resolved demo schemas + shared-lib absolute paths");
    if (schemas.empty() || sharedlib.empty()) return;

    // Fresh sandbox; HostBaker(".") writes the .part to <root>/parts/<hash>.part, so
    // a PartStore(<root>) (disk_path = <root>/parts/<hash>.part) reads the same file.
    const std::string root = "/tmp/me3_viewer_keepchildren";
    system(("rm -rf " + root).c_str());
    system(("mkdir -p " + root + "/parts").c_str());

    char prevcwd[4096]; if (!getcwd(prevcwd, sizeof prevcwd)) prevcwd[0] = '\0';
    CHECK(chdir(root.c_str()) == 0, "chdir into keep-children sandbox");

    script_host::ScriptHost host;
    host.set_shared_lib_root(sharedlib);
    pg::FileModuleResolver resolver(host, schemas);
    pg::HostBaker baker(host, ".");
    pg::PartGraph graph(resolver, baker);

    pg::InstallResult ir = graph.install({ pg::ChildRequest{ "Tree", pg::Params{} } });
    CHECK(ir.ok, "demo Tree installs into sandbox cache");
    if (!ir.ok) printf("  install error: %s\n", ir.error.c_str());

    // Recompute the Tree hash exactly as the graph did: Leaf (no children) folded
    // into Tree (kids,1). Use the SAME host (shared-lib root set) so the imported
    // module sources fold identically.
    uint64_t leaf_hash = host.resolve_hash(read_file(schemas + "/Leaf.js"), "{}");
    uint64_t kids[1] = { leaf_hash };
    uint64_t tree_hash = host.resolve_hash(read_file(schemas + "/Tree.js"), "{}", kids, 1);

    // PartStore reads <root>/parts/<hash>.part — point it at the sandbox root.
    viewer::PartStore store(".");
    const viewer::LoadedPart* lp = store.get_or_load(tree_hash);
    CHECK(lp != nullptr, "tree part loads from PartStore");
    CHECK(lp && !lp->children.empty(), "loaded tree part carries its child table");
    if (lp) printf("  loaded Tree carries %zu child instance(s)\n", lp->children.size());

    if (prevcwd[0]) (void)chdir(prevcwd);
    system(("rm -rf " + root).c_str());
}

int main() {
    test_world_state_delta();
    test_resolvers();
    test_part_store_missing();
    test_local_provider_cache();
    test_composer_counts();
    test_partstore_keeps_children();
    printf("\n%s\n", g_failures == 0 ? "viewer-logic OK" : "viewer-logic FAILED");
    return g_failures == 0 ? 0 : 1;
}
