# Part Artifact v2 (SP-1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the existing `.part` serialization to `format_version=2`, adding a content-addressed `compute_resolved_hash` helper, a child-instance table, and an ordered per-part LOD-level array, with a clean v2 cutover and no v1 reader.

**Architecture:** v2 keeps the v1 header/materials/BLAS-table/internal-instance sections byte-for-byte and appends two new length-prefixed sections (child instances, LOD levels). New `save_v2`/`load_v2` functions live alongside the v1 `save`/`load` in `part_asset.cpp`; the v1 `save`/`load` remain but are no longer the cutover path (v1 files fail the new loader's `format_version != 2` guard). The header carries `resolved_hash ^ kFormatVersion` and a `sizeof(ChildInstance)` layout guard; geometry is restored via `register_prebuilt`, child table and LOD array are passive (returned to caller).

**Tech Stack:** C++17, FNV-1a, existing BLASManager/TLASManager, headless GoogleTest-or-assert-style tests under MatterSurfaceLib/tests/ (match the existing part_asset_tests.cpp style — do NOT introduce a new test framework).

---

## File Structure

| File | Create/Modify | Responsibility |
|------|---------------|----------------|
| `MatterSurfaceLib/include/part_asset.h` | Modify | Add `kFormatVersionV2`, `compute_resolved_hash`, `ChildInstance`, `LodLevel`, `LodLevels`, `cache_path_resolved`, `save_v2`, `load_v2` declarations. |
| `MatterSurfaceLib/src/part_asset.cpp` | Modify | Implement `compute_resolved_hash`, `cache_path_resolved`, `save_v2`, `load_v2` alongside the existing v1 functions; add a shared `put_transform`/transform-read helper. |
| `MatterSurfaceLib/tests/part_asset_v2_tests.cpp` | Create | Headless assert-style tests covering every v2 testing bullet, mirroring `part_asset_tests.cpp` style. |
| `MatterSurfaceLib/tests/Makefile` | Modify | Add `PARTV2_TARGET = part_asset_v2_tests` TARGET/SOURCES/`run-partv2` rule and extend `clean`. |

---

## Tasks

### Task 1: Declare the v2 API surface in the header

**Files:**
- Modify: `MatterSurfaceLib/include/part_asset.h` (after line 36, the `compute_param_hash` decl; before `cache_path` at line 38)

- [ ] Add the v2 format-version constant just below the existing `kFormatVersion` (line 16). Edit `MatterSurfaceLib/include/part_asset.h`, inserting after line 16:
```cpp
constexpr uint32_t kFormatVersionV2 = 2u;
```

- [ ] Add the `ChildInstance` and LOD types plus the new function declarations. Insert into `namespace part_asset` after the `compute_param_hash` declaration (line 36), before `cache_path` (line 38):
```cpp
// Content-addressed identity for a part. All three inputs are OPAQUE byte ranges
// to SP-1 (script source, params, child resolved-hashes). child_hashes need NOT be
// pre-sorted; the helper sorts a local copy before folding so the result is
// order-independent over children. child_hashes may be null iff child_count == 0.
uint64_t compute_resolved_hash(const void* source_bytes, size_t source_len,
                               const void* params_bytes, size_t params_len,
                               const uint64_t* child_hashes, size_t child_count);

// Child-instance record: a reference to ANOTHER part by resolved hash + placement.
// transform is row-major, world placement under the parent's frame. Kept padding-free
// (8 + 64 = 72 bytes) so sizeof(ChildInstance) is a stable layout guard.
struct ChildInstance {
    uint64_t child_resolved_hash;
    float    transform[16];
};
static_assert(sizeof(ChildInstance) == 72,
              "ChildInstance must be padding-free for a stable layout guard");

// One LOD level: a screen-size selection threshold (projected pixel/normalized
// extent; SP-4 uses it, SP-1 only round-trips it) plus the BLAS-table indices that
// constitute the whole part at that detail.
struct LodLevel {
    float                 screen_size_threshold;
    std::vector<uint32_t> blas_indices;
};
// Ordered, coarsest-to-finest is SP-4's convention; SP-1 preserves array order as-is.
using LodLevels = std::vector<LodLevel>;

// Cache key / filename for a part keyed on its resolved hash: "parts/<16-hex>.part".
std::string cache_path_resolved(uint64_t resolved_hash);

// Serialize the baked managers + child table + LOD levels to path (atomic temp+rename).
// Writes format_version=2. Returns false on any I/O failure or dangling BLAS handle.
// GL-free. children may be null iff child_count == 0; lods may be empty.
bool save_v2(const std::string& path, const BLASManager& blas,
             const TLASManager& tlas,
             const ChildInstance* children, size_t child_count,
             const LodLevels& lods,
             uint64_t resolved_hash);

// Reconstruct managers from a v2 file; returns the child table and LOD levels to the
// caller (passive — no backend action). Returns false (caller regenerates) on any
// header/layout/material/corruption mismatch, format_version != 2, or I/O failure.
// expected_resolved_hash must equal the resolved hash the file was written with.
bool load_v2(const std::string& path, uint64_t expected_resolved_hash,
             BLASManager& blas, TLASManager& tlas,
             std::vector<ChildInstance>& children_out,
             LodLevels& lods_out);
```

- [ ] Add the `<vector>` include needed by `LodLevel`/`LodLevels`. Edit `MatterSurfaceLib/include/part_asset.h`, change the include block (lines 7-9) from:
```cpp
#include <cstddef>
#include <cstdint>
#include <string>
```
to:
```cpp
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
```

- [ ] Verify the header compiles standalone (syntax + static_assert) by compiling a throwaway TU. Run:
```bash
cd "/mnt/d/Shared With Desktop/AI/matter-engine-cpp/MatterSurfaceLib" && echo '#include "include/part_asset.h"
int main(){ return (int)sizeof(part_asset::ChildInstance); }' > /tmp/hdr_check.cpp && g++ -std=c++17 -fsyntax-only -Iinclude -I../../Libraries/raylib/src /tmp/hdr_check.cpp -DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 && echo HEADER_OK
```
Expected: prints `HEADER_OK` (no `static_assert` failure, no missing-type errors).

- [ ] Commit the header surface. Run:
```bash
cd "/mnt/d/Shared With Desktop/AI/matter-engine-cpp" && git add MatterSurfaceLib/include/part_asset.h && git commit -m "$(cat <<'EOF'
feat: declare part_asset v2 API (resolved hash, child instances, LOD levels)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: `compute_resolved_hash` helper (deterministic, order-independent, sensitive)

**Files:**
- Modify: `MatterSurfaceLib/src/part_asset.cpp` (add after `compute_param_hash`, lines 56-58)
- Test: `MatterSurfaceLib/tests/part_asset_v2_tests.cpp` (Create)

- [ ] Create the v2 test file with its harness scaffolding and the resolved-hash test, written FIRST (it will fail to link because `compute_resolved_hash` is undefined). Create `MatterSurfaceLib/tests/part_asset_v2_tests.cpp`:
```cpp
#include "../include/part_asset.h"
#include "../include/blas_manager.hpp"
#include "../include/tlas_manager.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); ++failures; } } while (0)

static void test_resolved_hash() {
    using namespace part_asset;
    const char* src = "function part(){ return cube(); }";
    const char* par = "\x01\x02\x03\x04";
    uint64_t kids[3] = { 0xAAAAull, 0xBBBBull, 0xCCCCull };

    // Deterministic: same inputs -> same hash.
    uint64_t h1 = compute_resolved_hash(src, strlen(src), par, 4, kids, 3);
    uint64_t h2 = compute_resolved_hash(src, strlen(src), par, 4, kids, 3);
    CHECK(h1 == h2, "resolved hash deterministic");

    // Order-independent over child hashes: shuffled children -> same hash.
    uint64_t shuffled[3] = { 0xCCCCull, 0xAAAAull, 0xBBBBull };
    uint64_t h3 = compute_resolved_hash(src, strlen(src), par, 4, shuffled, 3);
    CHECK(h1 == h3, "resolved hash order-independent over children");

    // Sensitive: changing source changes the hash.
    const char* src2 = "function part(){ return sphere(); }";
    uint64_t h4 = compute_resolved_hash(src2, strlen(src2), par, 4, kids, 3);
    CHECK(h1 != h4, "resolved hash changes when source changes");

    // Sensitive: changing params changes the hash.
    const char* par2 = "\x01\x02\x03\x05";
    uint64_t h5 = compute_resolved_hash(src, strlen(src), par2, 4, kids, 3);
    CHECK(h1 != h5, "resolved hash changes when params change");

    // Sensitive: changing a child hash changes the hash.
    uint64_t kids2[3] = { 0xAAAAull, 0xBBBBull, 0xDDDDull };
    uint64_t h6 = compute_resolved_hash(src, strlen(src), par, 4, kids2, 3);
    CHECK(h1 != h6, "resolved hash changes when a child hash changes");

    // Zero children is valid (null + 0).
    uint64_t h7 = compute_resolved_hash(src, strlen(src), par, 4, nullptr, 0);
    uint64_t h8 = compute_resolved_hash(src, strlen(src), par, 4, nullptr, 0);
    CHECK(h7 == h8, "resolved hash deterministic with zero children");
    CHECK(h7 != h1, "zero children differs from three children");
}

int main() {
    test_resolved_hash();
    if (failures == 0) printf("All part_asset_v2 tests passed\n");
    return failures == 0 ? 0 : 1;
}
```

- [ ] Add the Makefile rule so the test can be built (it will compile but FAIL TO LINK until the helper exists). Edit `MatterSurfaceLib/tests/Makefile`. First, extend the `.PHONY` line (line 67) and `clean` rule (line 70). Change line 67 from:
```make
.PHONY: clean run run-simp run-blas run-cell run-cont run-reg run-tint run-cull run-par run-cube run-ao run-part run-vox
```
to:
```make
.PHONY: clean run run-simp run-blas run-cell run-cont run-reg run-tint run-cull run-par run-cube run-ao run-part run-partv2 run-vox
```
Then change the `clean` rule body (line 70) from:
```make
	rm -f $(TARGET) $(SIMP_TARGET) $(BLAS_TARGET) $(CELL_TARGET) $(CONT_TARGET) $(REG_TARGET) $(TINT_TARGET) $(CULL_TARGET) $(PAR_TARGET) $(CUBE_TARGET) $(AO_TARGET) $(PART_TARGET) $(VOX_TARGET)
```
to:
```make
	rm -f $(TARGET) $(SIMP_TARGET) $(BLAS_TARGET) $(CELL_TARGET) $(CONT_TARGET) $(REG_TARGET) $(TINT_TARGET) $(CULL_TARGET) $(PAR_TARGET) $(CUBE_TARGET) $(AO_TARGET) $(PART_TARGET) $(PARTV2_TARGET) $(VOX_TARGET)
```

- [ ] Append the new target block at the END of `MatterSurfaceLib/tests/Makefile` (after the `run-vox` rule, line 253). The source list mirrors `PART_TARGET` exactly, swapping the test cpp:
```make

# Part Artifact v2 round-trip + resolved-hash + format-guard tests (headless, GL-free).
# Same source set as PART_TARGET; material_registry.c via gcc to keep extern "C" unmangled.
PARTV2_TARGET = part_asset_v2_tests
PARTV2_CPP = part_asset_v2_tests.cpp ../src/part_asset.cpp ../src/blas_manager.cpp \
             ../src/bvh.cpp ../src/tlas_manager.cpp ../src/vertex_ao.cpp \
             ../src/occupancy.cpp
PARTV2_C   = ../src/material_registry.c
PARTV2_C_OBJ = material_registry.o

$(PARTV2_TARGET): $(PARTV2_CPP) $(PARTV2_C)
	gcc -c $(PARTV2_C) -O2 -DPLATFORM_DESKTOP $(INCLUDE_PATHS)
	$(CC) $(PARTV2_CPP) $(PARTV2_C_OBJ) -o $(PARTV2_TARGET) $(CFLAGS) $(INCLUDE_PATHS) $(LDFLAGS) $(LDLIBS)
	rm -f $(PARTV2_C_OBJ)

run-partv2: $(PARTV2_TARGET)
	./$(PARTV2_TARGET)
```

- [ ] Run the test and confirm it FAILS at link (helper not yet defined). Run:
```bash
make -C "/mnt/d/Shared With Desktop/AI/matter-engine-cpp/MatterSurfaceLib/tests" run-partv2
```
Expected: linker error `undefined reference to 'part_asset::compute_resolved_hash(...)'`.

- [ ] Implement `compute_resolved_hash` in `MatterSurfaceLib/src/part_asset.cpp`. Add `#include <algorithm>` to the includes (after line 7, `#include <unordered_map>`), then add the function after `compute_param_hash` (line 58):
```cpp
uint64_t compute_resolved_hash(const void* source_bytes, size_t source_len,
                               const void* params_bytes, size_t params_len,
                               const uint64_t* child_hashes, size_t child_count) {
    // Fold source, then params, then sorted child hashes into one rolling FNV-1a.
    // Reuse fnv1a64's basis/prime by chaining: hash the concatenation manually so
    // the stream order (source -> params -> sorted children) is fixed.
    uint64_t h = 1469598103934665603ull; // FNV offset basis
    auto fold = [&h](const void* data, size_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    };
    fold(source_bytes, source_len);
    fold(params_bytes, params_len);
    std::vector<uint64_t> sorted(child_hashes, child_hashes + child_count);
    std::sort(sorted.begin(), sorted.end()); // order-independent over children
    for (uint64_t c : sorted) fold(&c, sizeof(c));
    return h;
}
```

- [ ] Run the test and confirm it PASSES. Run:
```bash
make -C "/mnt/d/Shared With Desktop/AI/matter-engine-cpp/MatterSurfaceLib/tests" run-partv2
```
Expected: `All part_asset_v2 tests passed`, exit 0.

- [ ] Commit. Run:
```bash
cd "/mnt/d/Shared With Desktop/AI/matter-engine-cpp" && git add MatterSurfaceLib/src/part_asset.cpp MatterSurfaceLib/tests/part_asset_v2_tests.cpp MatterSurfaceLib/tests/Makefile && git commit -m "$(cat <<'EOF'
feat: add compute_resolved_hash with order-independent child folding

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: `cache_path_resolved` filename helper

**Files:**
- Modify: `MatterSurfaceLib/src/part_asset.cpp` (add after `cache_path`, lines 60-64)
- Test: `MatterSurfaceLib/tests/part_asset_v2_tests.cpp`

- [ ] Add the failing test. In `MatterSurfaceLib/tests/part_asset_v2_tests.cpp`, add this function above `main()`:
```cpp
static void test_cache_path_resolved() {
    using namespace part_asset;
    CHECK(cache_path_resolved(0x1ull) == "parts/0000000000000001.part",
          "cache_path_resolved zero-padded hex");
    CHECK(cache_path_resolved(0xDEADBEEFCAFEBABEull) == "parts/deadbeefcafebabe.part",
          "cache_path_resolved full-width hex");
}
```
and add `test_cache_path_resolved();` as the first line inside `main()` (before `test_resolved_hash();`).

- [ ] Run and confirm FAIL at link. Run:
```bash
make -C "/mnt/d/Shared With Desktop/AI/matter-engine-cpp/MatterSurfaceLib/tests" run-partv2
```
Expected: linker error `undefined reference to 'part_asset::cache_path_resolved(unsigned long)'`.

- [ ] Implement it. In `MatterSurfaceLib/src/part_asset.cpp`, add after the existing `cache_path` (line 64):
```cpp
std::string cache_path_resolved(uint64_t resolved_hash) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016llx",
                  static_cast<unsigned long long>(resolved_hash));
    return std::string("parts/") + buf + ".part";
}
```

- [ ] Run and confirm PASS. Run:
```bash
make -C "/mnt/d/Shared With Desktop/AI/matter-engine-cpp/MatterSurfaceLib/tests" run-partv2
```
Expected: `All part_asset_v2 tests passed`.

- [ ] Commit. Run:
```bash
cd "/mnt/d/Shared With Desktop/AI/matter-engine-cpp" && git add MatterSurfaceLib/src/part_asset.cpp MatterSurfaceLib/tests/part_asset_v2_tests.cpp && git commit -m "$(cat <<'EOF'
feat: add cache_path_resolved keyed on a part's resolved hash

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: `save_v2` writes the header + v1 sections + child/LOD sections

**Files:**
- Modify: `MatterSurfaceLib/src/part_asset.cpp` (add `save_v2` after `save`, line 130)
- Test: `MatterSurfaceLib/tests/part_asset_v2_tests.cpp`

- [ ] Add test scaffolding (scene builder + file readers, mirrored from `part_asset_tests.cpp`) and a header-level save assertion. In `MatterSurfaceLib/tests/part_asset_v2_tests.cpp`, add these helpers above `main()`:
```cpp
static Tri ptri(float ox, float oy) {
    Tri t;
    t.vertex0 = make_float3(ox + 0.0f, oy + 0.0f, 0.0f);
    t.vertex1 = make_float3(ox + 1.0f, oy + 0.0f, 0.0f);
    t.vertex2 = make_float3(ox + 0.0f, oy + 1.0f, 0.0f);
    t.centroid = make_float3(ox + 0.333f, oy + 0.333f, 0.0f);
    return t;
}

static void build_scene(BLASManager& blas, TLASManager& tlas,
                        BLASHandle& hA, BLASHandle& hB) {
    Tri triA[3] = { ptri(0,0), ptri(5,0), ptri(0,5) };
    Tri triB[2] = { ptri(20,0), ptri(25,5) };
    TriEx exA[3] = {}; TriEx exB[2] = {};
    for (auto& e : exA) { e.materialId = 8; e.N0=e.N1=e.N2=make_float3(0,0,1); e.tint=make_float4(1,1,1,0); }
    for (auto& e : exB) { e.materialId = 9; e.N0=e.N1=e.N2=make_float3(0,0,1); e.tint=make_float4(1,1,1,0); }
    hA = blas.register_triangles(triA, 3, exA);
    hB = blas.register_triangles(triB, 2, exB);

    std::vector<TLASManager::DrawInstance> insts(3);
    insts[0].blas_handle = hA; insts[0].material_id = 8; insts[0].transform = Matrix4x4();
    insts[1].blas_handle = hB; insts[1].material_id = 9; insts[1].transform = Matrix4x4();
    insts[1].transform.m[3] = 10.0f;
    insts[2].blas_handle = hA; insts[2].material_id = 8; insts[2].transform = Matrix4x4();
    insts[2].transform.m[7] = 7.0f;
    tlas.draw_batch(insts);
    tlas.build(blas);
}

// A couple of synthetic child rows and a 2-level LOD array for the full round-trip.
static std::vector<part_asset::ChildInstance> sample_children() {
    std::vector<part_asset::ChildInstance> kids(2);
    kids[0].child_resolved_hash = 0x1111222233334444ull;
    for (int i = 0; i < 16; ++i) kids[0].transform[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    kids[0].transform[3] = 2.5f;
    kids[1].child_resolved_hash = 0x5555666677778888ull;
    for (int i = 0; i < 16; ++i) kids[1].transform[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    kids[1].transform[7] = -4.0f;
    return kids;
}
static part_asset::LodLevels sample_lods() {
    part_asset::LodLevels lods(2);
    lods[0].screen_size_threshold = 256.0f;
    lods[0].blas_indices = { 0u, 1u };
    lods[1].screen_size_threshold = 32.0f;
    lods[1].blas_indices = { 0u };
    return lods;
}

static uint32_t rd_u32(const std::vector<uint8_t>& b, size_t off) {
    uint32_t v; memcpy(&v, b.data()+off, 4); return v;
}
static uint64_t rd_u64(const std::vector<uint8_t>& b, size_t off) {
    uint64_t v; memcpy(&v, b.data()+off, 8); return v;
}
static std::vector<uint8_t> read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return {};
    fseek(f,0,SEEK_END); long n = ftell(f); fseek(f,0,SEEK_SET);
    std::vector<uint8_t> b(n);
    size_t got = fread(b.data(),1,n,f); fclose(f);
    b.resize(got);
    return b;
}
static void write_file(const char* path, const std::vector<uint8_t>& b) {
    FILE* f = fopen(path, "wb"); fwrite(b.data(),1,b.size(),f); fclose(f);
}

static void test_save_v2_header() {
    using namespace part_asset;
    BLASManager blas; TLASManager tlas(64);
    BLASHandle hA, hB; build_scene(blas, tlas, hA, hB);
    auto kids = sample_children();
    auto lods = sample_lods();

    const char* path = "test_v2_save.part";
    remove(path);
    bool ok = save_v2(path, blas, tlas, kids.data(), kids.size(), lods, 0xABCDEF12u);
    CHECK(ok, "save_v2 returns true");

    std::vector<uint8_t> b = read_file(path);
    // Header layout (v2): magic u32, version u32, resolved_hash^ver u64, sizeof Tri/TriEx/
    // BVHNode/ChildInstance u32 x4, content_hash u64 => 8 + 16 + 8 = 40-byte header.
    CHECK(b.size() >= 40, "file has at least a v2 header");
    CHECK(rd_u32(b, 0) == kMagic, "magic written");
    CHECK(rd_u32(b, 4) == kFormatVersionV2, "v2 version written");
    CHECK(rd_u64(b, 8) == (0xABCDEF12ull ^ (uint64_t)kFormatVersionV2),
          "resolved hash stored XOR format version");
    CHECK(rd_u32(b, 16) == (uint32_t)sizeof(Tri), "sizeof Tri written");
    CHECK(rd_u32(b, 20) == (uint32_t)sizeof(TriEx), "sizeof TriEx written");
    CHECK(rd_u32(b, 24) == (uint32_t)sizeof(BVHNode), "sizeof BVHNode written");
    CHECK(rd_u32(b, 28) == (uint32_t)sizeof(ChildInstance), "sizeof ChildInstance written");
    uint64_t stored = rd_u64(b, 32);
    uint64_t recomputed = fnv1a64(b.data()+40, b.size()-40);
    CHECK(stored == recomputed, "content hash covers body after 40-byte header");

    remove(path);
}
```
and add `test_save_v2_header();` to `main()` after `test_resolved_hash();`.

- [ ] Run and confirm FAIL at link (`save_v2` undefined). Run:
```bash
make -C "/mnt/d/Shared With Desktop/AI/matter-engine-cpp/MatterSurfaceLib/tests" run-partv2
```
Expected: linker error `undefined reference to 'part_asset::save_v2(...)'`.

- [ ] Implement `save_v2` in `MatterSurfaceLib/src/part_asset.cpp`. Add after the existing `save` (closing brace at line 130). The materials/BLAS/internal-instance sections are copied from `save` verbatim; the new child and LOD sections are appended; the header gains the `sizeof(ChildInstance)` guard and writes `resolved_hash ^ kFormatVersionV2`:
```cpp
bool save_v2(const std::string& path, const BLASManager& blas,
             const TLASManager& tlas,
             const ChildInstance* children, size_t child_count,
             const LodLevels& lods,
             uint64_t resolved_hash) {
    std::vector<uint8_t> body;

    // --- Materials --- (unchanged from v1)
    const uint32_t mcount = static_cast<uint32_t>(MaterialRegistryCount());
    put<uint32_t>(body, mcount);
    for (uint32_t i = 0; i < mcount; ++i)
        put_bytes(body, MaterialRegistryGet(static_cast<int>(i)), sizeof(MaterialDef));

    // --- BLAS table --- (unchanged from v1; index == position in entries_)
    const auto& entries = blas.get_entries();
    put<uint32_t>(body, static_cast<uint32_t>(entries.size()));
    std::unordered_map<BLASHandle, uint32_t> handle_to_index;
    for (uint32_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        handle_to_index[e->handle] = i;
        const uint32_t tri_count  = static_cast<uint32_t>(e->mesh->triCount);
        const uint32_t nodes_used = e->bvh->nodesUsed;
        const uint32_t has_triex  = e->mesh->triEx ? 1u : 0u;
        put<uint32_t>(body, e->hash);
        put<uint32_t>(body, e->ref_count);
        put<uint32_t>(body, tri_count);
        put<uint32_t>(body, nodes_used);
        put<uint32_t>(body, has_triex);
        put_bytes(body, e->triangles.data(), tri_count * sizeof(Tri));
        if (has_triex) put_bytes(body, e->mesh->triEx, tri_count * sizeof(TriEx));
        put_bytes(body, e->bvh->bvhNode, nodes_used * sizeof(BVHNode));
        put_bytes(body, e->bvh->triIdx,  tri_count  * sizeof(uint));
    }

    // --- Internal instances --- (unchanged from v1)
    const auto& recs = tlas.get_draw_records();
    put<uint32_t>(body, static_cast<uint32_t>(recs.size()));
    for (const auto& r : recs) {
        auto it = handle_to_index.find(r.blas_handle);
        if (it == handle_to_index.end()) return false; // dangling handle
        put<uint32_t>(body, it->second);
        put<uint32_t>(body, r.material_id);
        put_bytes(body, r.transform.m, 16 * sizeof(float));
    }

    // --- Child instances --- (NEW; references to other parts by resolved hash)
    put<uint32_t>(body, static_cast<uint32_t>(child_count));
    for (size_t i = 0; i < child_count; ++i) {
        put<uint64_t>(body, children[i].child_resolved_hash);
        put_bytes(body, children[i].transform, 16 * sizeof(float));
    }

    // --- LOD levels --- (NEW; ordered, may be empty)
    put<uint32_t>(body, static_cast<uint32_t>(lods.size()));
    for (const auto& lvl : lods) {
        put<float>(body, lvl.screen_size_threshold);
        put<uint32_t>(body, static_cast<uint32_t>(lvl.blas_indices.size()));
        for (uint32_t idx : lvl.blas_indices) put<uint32_t>(body, idx);
    }

    // --- Header (40 bytes) ---
    const uint64_t content_hash = fnv1a64(body.data(), body.size());
    std::vector<uint8_t> head;
    put<uint32_t>(head, kMagic);
    put<uint32_t>(head, kFormatVersionV2);
    put<uint64_t>(head, resolved_hash ^ static_cast<uint64_t>(kFormatVersionV2));
    put<uint32_t>(head, static_cast<uint32_t>(sizeof(Tri)));
    put<uint32_t>(head, static_cast<uint32_t>(sizeof(TriEx)));
    put<uint32_t>(head, static_cast<uint32_t>(sizeof(BVHNode)));
    put<uint32_t>(head, static_cast<uint32_t>(sizeof(ChildInstance)));
    put<uint64_t>(head, content_hash);

    // --- Atomic write --- (unchanged from v1)
    ensure_parent_dir(path);
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(head.data(), 1, head.size(), f) == head.size() &&
              std::fwrite(body.data(), 1, body.size(), f) == body.size();
    std::fclose(f);
    if (!ok) { std::remove(tmp.c_str()); return false; }
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}
```

- [ ] Run and confirm PASS. Run:
```bash
make -C "/mnt/d/Shared With Desktop/AI/matter-engine-cpp/MatterSurfaceLib/tests" run-partv2
```
Expected: `All part_asset_v2 tests passed`.

- [ ] Commit. Run:
```bash
cd "/mnt/d/Shared With Desktop/AI/matter-engine-cpp" && git add MatterSurfaceLib/src/part_asset.cpp MatterSurfaceLib/tests/part_asset_v2_tests.cpp && git commit -m "$(cat <<'EOF'
feat: implement part_asset save_v2 with child-instance and LOD sections

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: `load_v2` restores managers + returns child/LOD sections (full round-trip)

**Files:**
- Modify: `MatterSurfaceLib/src/part_asset.cpp` (add `load_v2` after `load`, line 221)
- Test: `MatterSurfaceLib/tests/part_asset_v2_tests.cpp`

- [ ] Add the full round-trip test. In `MatterSurfaceLib/tests/part_asset_v2_tests.cpp`, add above `main()`:
```cpp
static void test_round_trip_full() {
    using namespace part_asset;

    // Source scene + children + LODs.
    BLASManager blasA; TLASManager tlasA(64);
    BLASHandle hA, hB; build_scene(blasA, tlasA, hA, hB);
    auto kids = sample_children();
    auto lods = sample_lods();

    std::vector<Tri> triA; blasA.generate_triangle_data(triA);
    std::vector<LegacyBVHNode> nodeA; blasA.generate_node_data(nodeA);
    const auto recsA = tlasA.get_draw_records();

    const char* path = "test_v2_round.part";
    remove(path);
    CHECK(save_v2(path, blasA, tlasA, kids.data(), kids.size(), lods, 0x55AA55AAu),
          "round-trip save_v2 ok");

    // Load into fresh state.
    BLASManager blasB; TLASManager tlasB(64);
    std::vector<ChildInstance> kidsOut;
    LodLevels lodsOut;
    bool ok = load_v2(path, 0x55AA55AAu, blasB, tlasB, kidsOut, lodsOut);
    CHECK(ok, "round-trip load_v2 ok");

    // BLAS CPU data byte-identical.
    std::vector<Tri> triB; blasB.generate_triangle_data(triB);
    std::vector<LegacyBVHNode> nodeB; blasB.generate_node_data(nodeB);
    CHECK(triA.size() == triB.size() &&
          memcmp(triA.data(), triB.data(), triA.size()*sizeof(Tri)) == 0,
          "round-trip triangle bytes");
    CHECK(nodeA.size() == nodeB.size() &&
          memcmp(nodeA.data(), nodeB.data(), nodeA.size()*sizeof(LegacyBVHNode)) == 0,
          "round-trip node bytes");

    // Internal instances preserved.
    const auto recsB = tlasB.get_draw_records();
    CHECK(recsA.size() == recsB.size() && recsB.size() == 3, "round-trip instance count");
    bool inst_ok = recsA.size() == recsB.size();
    for (size_t i = 0; inst_ok && i < recsA.size(); ++i) {
        if (recsA[i].material_id != recsB[i].material_id) inst_ok = false;
        if (memcmp(recsA[i].transform.m, recsB[i].transform.m, 16*sizeof(float)) != 0) inst_ok = false;
    }
    CHECK(inst_ok, "round-trip instance material+transform");

    // Child instances preserved exactly.
    CHECK(kidsOut.size() == kids.size() && kidsOut.size() == 2, "round-trip child count");
    bool kids_ok = kidsOut.size() == kids.size();
    for (size_t i = 0; kids_ok && i < kids.size(); ++i) {
        if (kidsOut[i].child_resolved_hash != kids[i].child_resolved_hash) kids_ok = false;
        if (memcmp(kidsOut[i].transform, kids[i].transform, 16*sizeof(float)) != 0) kids_ok = false;
    }
    CHECK(kids_ok, "round-trip child hash+transform");

    // LOD levels preserved exactly (order, threshold, index arrays).
    CHECK(lodsOut.size() == lods.size() && lodsOut.size() == 2, "round-trip lod count");
    bool lod_ok = lodsOut.size() == lods.size();
    for (size_t i = 0; lod_ok && i < lods.size(); ++i) {
        if (lodsOut[i].screen_size_threshold != lods[i].screen_size_threshold) lod_ok = false;
        if (lodsOut[i].blas_indices != lods[i].blas_indices) lod_ok = false;
    }
    CHECK(lod_ok, "round-trip lod threshold+indices");

    // Wrong expected resolved hash must be rejected.
    BLASManager blasC; TLASManager tlasC(64);
    std::vector<ChildInstance> kc; LodLevels lc;
    CHECK(!load_v2(path, 0xDEADBEEFu, blasC, tlasC, kc, lc),
          "load_v2 rejects wrong resolved hash");

    remove(path);
}
```
and add `test_round_trip_full();` to `main()` after `test_save_v2_header();`.

- [ ] Run and confirm FAIL at link (`load_v2` undefined). Run:
```bash
make -C "/mnt/d/Shared With Desktop/AI/matter-engine-cpp/MatterSurfaceLib/tests" run-partv2
```
Expected: linker error `undefined reference to 'part_asset::load_v2(...)'`.

- [ ] Implement `load_v2` in `MatterSurfaceLib/src/part_asset.cpp`. Add after the existing `load` (closing brace at line 221). Header/materials/BLAS/internal-instance reads mirror `load`; the header uses the 40-byte layout with the `sizeof(ChildInstance)` guard and un-XORs the resolved hash; the new sections are read into the out-params:
```cpp
bool load_v2(const std::string& path, uint64_t expected_resolved_hash,
             BLASManager& blas, TLASManager& tlas,
             std::vector<ChildInstance>& children_out,
             LodLevels& lods_out) {
    children_out.clear();
    lods_out.clear();

    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz < 40) { std::fclose(f); return false; } // 40-byte v2 header
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    bool read_ok = std::fread(buf.data(), 1, buf.size(), f) == buf.size();
    std::fclose(f);
    if (!read_ok) return false;

    Reader r{ buf.data(), buf.data() + buf.size() };

    // --- Header + validation ---
    const uint32_t magic    = r.get<uint32_t>();
    const uint32_t version  = r.get<uint32_t>();
    const uint64_t rhash_x  = r.get<uint64_t>();
    const uint32_t s_tri    = r.get<uint32_t>();
    const uint32_t s_triex  = r.get<uint32_t>();
    const uint32_t s_node   = r.get<uint32_t>();
    const uint32_t s_child  = r.get<uint32_t>();
    const uint64_t content  = r.get<uint64_t>();
    if (!r.ok) return false;
    if (magic   != kMagic)                  return false;
    if (version != kFormatVersionV2)        return false; // v1 cutover: v1 fails here
    if (s_tri   != sizeof(Tri))             return false;
    if (s_triex != sizeof(TriEx))           return false;
    if (s_node  != sizeof(BVHNode))         return false;
    if (s_child != sizeof(ChildInstance))   return false;
    const uint64_t resolved = rhash_x ^ static_cast<uint64_t>(kFormatVersionV2);
    if (resolved != expected_resolved_hash) return false;
    if (fnv1a64(r.p, static_cast<size_t>(r.end - r.p)) != content) return false;

    // --- Materials (validate against the live registry) ---
    const uint32_t mcount = r.get<uint32_t>();
    if (!r.ok) return false;
    if (static_cast<int>(mcount) != MaterialRegistryCount()) return false;
    for (uint32_t i = 0; i < mcount; ++i) {
        const uint8_t* md = r.take(sizeof(MaterialDef));
        if (!r.ok) return false;
        if (std::memcmp(md, MaterialRegistryGet(static_cast<int>(i)), sizeof(MaterialDef)) != 0)
            return false;
    }

    // --- BLAS table ---
    const uint32_t blas_count = r.get<uint32_t>();
    if (!r.ok) return false;
    std::vector<BLASHandle> handles(blas_count, INVALID_BLAS_HANDLE);
    for (uint32_t i = 0; i < blas_count; ++i) {
        const uint32_t hash       = r.get<uint32_t>();
        const uint32_t ref_count  = r.get<uint32_t>();
        const uint32_t tri_count  = r.get<uint32_t>();
        const uint32_t nodes_used = r.get<uint32_t>();
        const uint32_t has_triex  = r.get<uint32_t>();
        if (!r.ok) return false;
        const Tri*     tris  = reinterpret_cast<const Tri*>(r.take(tri_count * sizeof(Tri)));
        const TriEx*   triex = has_triex
                               ? reinterpret_cast<const TriEx*>(r.take(tri_count * sizeof(TriEx)))
                               : nullptr;
        const BVHNode* nodes  = reinterpret_cast<const BVHNode*>(r.take(nodes_used * sizeof(BVHNode)));
        const uint*    triIdx = reinterpret_cast<const uint*>(r.take(tri_count * sizeof(uint)));
        if (!r.ok) return false;
        handles[i] = blas.register_prebuilt(tris, triex, static_cast<int>(tri_count),
                                            nodes, nodes_used, triIdx, hash, ref_count);
        if (handles[i] == INVALID_BLAS_HANDLE) return false;
    }

    // --- Internal instances ---
    const uint32_t inst_count = r.get<uint32_t>();
    if (!r.ok) return false;
    std::vector<TLASManager::DrawInstance> insts;
    insts.reserve(inst_count);
    for (uint32_t i = 0; i < inst_count; ++i) {
        const uint32_t blas_index = r.get<uint32_t>();
        const uint32_t material   = r.get<uint32_t>();
        const uint8_t* tf         = r.take(16 * sizeof(float));
        if (!r.ok) return false;
        if (blas_index >= blas_count) return false;
        TLASManager::DrawInstance di;
        di.blas_handle = handles[blas_index];
        di.material_id = material;
        std::memcpy(di.transform.m, tf, 16 * sizeof(float));
        insts.push_back(di);
    }

    // --- Child instances (NEW; passive — returned to caller) ---
    const uint32_t child_count = r.get<uint32_t>();
    if (!r.ok) return false;
    children_out.reserve(child_count);
    for (uint32_t i = 0; i < child_count; ++i) {
        ChildInstance ci{};
        ci.child_resolved_hash = r.get<uint64_t>();
        const uint8_t* tf = r.take(16 * sizeof(float));
        if (!r.ok) return false;
        std::memcpy(ci.transform, tf, 16 * sizeof(float));
        children_out.push_back(ci);
    }

    // --- LOD levels (NEW; passive — returned to caller) ---
    const uint32_t level_count = r.get<uint32_t>();
    if (!r.ok) return false;
    lods_out.reserve(level_count);
    for (uint32_t i = 0; i < level_count; ++i) {
        LodLevel lvl;
        lvl.screen_size_threshold = r.get<float>();
        const uint32_t idx_count  = r.get<uint32_t>();
        if (!r.ok) return false;
        lvl.blas_indices.reserve(idx_count);
        for (uint32_t j = 0; j < idx_count; ++j) {
            const uint32_t idx = r.get<uint32_t>();
            if (!r.ok) return false;
            if (idx >= blas_count) return false; // dangling LOD index: regenerate
            lvl.blas_indices.push_back(idx);
        }
        lods_out.push_back(std::move(lvl));
    }
    if (!r.ok) return false;

    if (!insts.empty()) {
        tlas.draw_batch(insts);
        tlas.build(blas);
    }
    return true;
}
```

- [ ] Run and confirm PASS. Run:
```bash
make -C "/mnt/d/Shared With Desktop/AI/matter-engine-cpp/MatterSurfaceLib/tests" run-partv2
```
Expected: `All part_asset_v2 tests passed`.

- [ ] Commit. Run:
```bash
cd "/mnt/d/Shared With Desktop/AI/matter-engine-cpp" && git add MatterSurfaceLib/src/part_asset.cpp MatterSurfaceLib/tests/part_asset_v2_tests.cpp && git commit -m "$(cat <<'EOF'
feat: implement part_asset load_v2 restoring managers + child/LOD sections

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: Degenerate LOD round-trips (empty + single-level)

**Files:**
- Test: `MatterSurfaceLib/tests/part_asset_v2_tests.cpp`

- [ ] Add the degenerate-LOD test. In `MatterSurfaceLib/tests/part_asset_v2_tests.cpp`, add above `main()`:
```cpp
static void test_round_trip_degenerate_lod() {
    using namespace part_asset;
    BLASManager blasA; TLASManager tlasA(64);
    BLASHandle hA, hB; build_scene(blasA, tlasA, hA, hB);
    auto kids = sample_children();

    // Empty LOD array round-trips.
    {
        LodLevels empty;
        const char* path = "test_v2_lod_empty.part";
        remove(path);
        CHECK(save_v2(path, blasA, tlasA, kids.data(), kids.size(), empty, 0x10u),
              "empty-LOD save ok");
        BLASManager b; TLASManager t(64);
        std::vector<ChildInstance> ko; LodLevels lo;
        CHECK(load_v2(path, 0x10u, b, t, ko, lo), "empty-LOD load ok");
        CHECK(lo.empty(), "empty LOD array round-trips empty");
        CHECK(ko.size() == 2, "children still round-trip with empty LOD");
        remove(path);
    }

    // Single-level LOD round-trips.
    {
        LodLevels one(1);
        one[0].screen_size_threshold = 128.0f;
        one[0].blas_indices = { 1u };
        const char* path = "test_v2_lod_one.part";
        remove(path);
        CHECK(save_v2(path, blasA, tlasA, kids.data(), kids.size(), one, 0x11u),
              "single-LOD save ok");
        BLASManager b; TLASManager t(64);
        std::vector<ChildInstance> ko; LodLevels lo;
        CHECK(load_v2(path, 0x11u, b, t, ko, lo), "single-LOD load ok");
        CHECK(lo.size() == 1, "single LOD level round-trips");
        CHECK(lo.size() == 1 && lo[0].screen_size_threshold == 128.0f,
              "single LOD threshold preserved");
        CHECK(lo.size() == 1 && lo[0].blas_indices == std::vector<uint32_t>{1u},
              "single LOD indices preserved");
        remove(path);
    }
}
```
and add `test_round_trip_degenerate_lod();` to `main()` after `test_round_trip_full();`.

- [ ] Run and confirm PASS (no implementation change needed; this exercises existing code). Run:
```bash
make -C "/mnt/d/Shared With Desktop/AI/matter-engine-cpp/MatterSurfaceLib/tests" run-partv2
```
Expected: `All part_asset_v2 tests passed`.

- [ ] Commit. Run:
```bash
cd "/mnt/d/Shared With Desktop/AI/matter-engine-cpp" && git add MatterSurfaceLib/tests/part_asset_v2_tests.cpp && git commit -m "$(cat <<'EOF'
test: cover empty and single-level LOD round-trip for part v2

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: Zero-children round-trip parity

**Files:**
- Test: `MatterSurfaceLib/tests/part_asset_v2_tests.cpp`

- [ ] Add a test that an empty child table (null + 0) round-trips and that `register_prebuilt` restores byte-identical geometry (prebuilt-vs-built parity carried over from v1, exercised through the v2 path). In `MatterSurfaceLib/tests/part_asset_v2_tests.cpp`, add above `main()`:
```cpp
static void test_round_trip_no_children() {
    using namespace part_asset;
    BLASManager blasA; TLASManager tlasA(64);
    BLASHandle hA, hB; build_scene(blasA, tlasA, hA, hB);
    auto lods = sample_lods();

    std::vector<Tri> triA; blasA.generate_triangle_data(triA);
    std::vector<LegacyBVHNode> nodeA; blasA.generate_node_data(nodeA);

    const char* path = "test_v2_nokids.part";
    remove(path);
    CHECK(save_v2(path, blasA, tlasA, nullptr, 0, lods, 0x20u),
          "no-children save ok");

    BLASManager blasB; TLASManager tlasB(64);
    std::vector<ChildInstance> ko; LodLevels lo;
    CHECK(load_v2(path, 0x20u, blasB, tlasB, ko, lo), "no-children load ok");
    CHECK(ko.empty(), "empty child table round-trips empty");

    // prebuilt-vs-built parity: geometry restored via register_prebuilt is
    // byte-identical to the source built BVH.
    std::vector<Tri> triB; blasB.generate_triangle_data(triB);
    std::vector<LegacyBVHNode> nodeB; blasB.generate_node_data(nodeB);
    CHECK(triA.size() == triB.size() &&
          memcmp(triA.data(), triB.data(), triA.size()*sizeof(Tri)) == 0,
          "prebuilt-vs-built triangle parity through v2");
    CHECK(nodeA.size() == nodeB.size() &&
          memcmp(nodeA.data(), nodeB.data(), nodeA.size()*sizeof(LegacyBVHNode)) == 0,
          "prebuilt-vs-built node parity through v2");

    remove(path);
}
```
and add `test_round_trip_no_children();` to `main()` after `test_round_trip_degenerate_lod();`.

- [ ] Run and confirm PASS. Run:
```bash
make -C "/mnt/d/Shared With Desktop/AI/matter-engine-cpp/MatterSurfaceLib/tests" run-partv2
```
Expected: `All part_asset_v2 tests passed`.

- [ ] Commit. Run:
```bash
cd "/mnt/d/Shared With Desktop/AI/matter-engine-cpp" && git add MatterSurfaceLib/tests/part_asset_v2_tests.cpp && git commit -m "$(cat <<'EOF'
test: cover zero-children round-trip and prebuilt-vs-built parity via v2

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

### Task 8: Layout, corruption, and v1-cutover guards

**Files:**
- Test: `MatterSurfaceLib/tests/part_asset_v2_tests.cpp`

- [ ] Add the guard tests. In `MatterSurfaceLib/tests/part_asset_v2_tests.cpp`, add above `main()`. Header offsets: magic@0, version@4, resolved^ver@8, sizeofTri@16, sizeofTriEx@20, sizeofBVHNode@24, sizeofChildInstance@28, content_hash@32, body starts@40:
```cpp
static void test_v2_guards() {
    using namespace part_asset;
    BLASManager blasA; TLASManager tlasA(64);
    BLASHandle hA, hB; build_scene(blasA, tlasA, hA, hB);
    auto kids = sample_children();
    auto lods = sample_lods();

    const char* path = "test_v2_guard.part";
    remove(path);
    CHECK(save_v2(path, blasA, tlasA, kids.data(), kids.size(), lods, 0x1234u),
          "guard save ok");
    std::vector<uint8_t> good = read_file(path);

    // Sanity: unmodified file loads.
    { BLASManager b; TLASManager t(64); std::vector<ChildInstance> ko; LodLevels lo;
      CHECK(load_v2(path, 0x1234u, b, t, ko, lo), "unmodified v2 file loads"); }

    // Layout guard: corrupt sizeof_ChildInstance (offset 28).
    { auto bad = good; uint32_t v = rd_u32(bad,28) + 1; memcpy(bad.data()+28,&v,4);
      write_file(path, bad);
      BLASManager b; TLASManager t(64); std::vector<ChildInstance> ko; LodLevels lo;
      CHECK(!load_v2(path, 0x1234u, b, t, ko, lo), "rejects sizeof_ChildInstance mismatch"); }

    // Layout guard: corrupt sizeof_Tri (offset 16).
    { auto bad = good; uint32_t v = rd_u32(bad,16) + 1; memcpy(bad.data()+16,&v,4);
      write_file(path, bad);
      BLASManager b; TLASManager t(64); std::vector<ChildInstance> ko; LodLevels lo;
      CHECK(!load_v2(path, 0x1234u, b, t, ko, lo), "rejects sizeof_Tri mismatch"); }

    // Version guard / v1 cutover: a format_version=1 file is rejected by the v2 loader.
    { auto bad = good; uint32_t v = 1u; memcpy(bad.data()+4,&v,4);
      write_file(path, bad);
      BLASManager b; TLASManager t(64); std::vector<ChildInstance> ko; LodLevels lo;
      CHECK(!load_v2(path, 0x1234u, b, t, ko, lo), "v2 loader rejects v1 (format_version=1)"); }

    // Corruption guard: flip a byte deep in the body (child or LOD section).
    // The body is at least the header(40) + materials + BLAS + instances; flipping
    // the last byte lands inside the trailing LOD section.
    { auto bad = good; bad[bad.size()-1] ^= 0xFF;
      write_file(path, bad);
      BLASManager b; TLASManager t(64); std::vector<ChildInstance> ko; LodLevels lo;
      CHECK(!load_v2(path, 0x1234u, b, t, ko, lo), "rejects trailing-section corruption"); }

    // Magic guard.
    { auto bad = good; bad[0] ^= 0xFF;
      write_file(path, bad);
      BLASManager b; TLASManager t(64); std::vector<ChildInstance> ko; LodLevels lo;
      CHECK(!load_v2(path, 0x1234u, b, t, ko, lo), "rejects bad magic"); }

    remove(path);
}
```
and add `test_v2_guards();` to `main()` after `test_round_trip_no_children();`.

- [ ] Run and confirm PASS (guards are already implemented in `load_v2`). Run:
```bash
make -C "/mnt/d/Shared With Desktop/AI/matter-engine-cpp/MatterSurfaceLib/tests" run-partv2
```
Expected: `All part_asset_v2 tests passed`.

- [ ] Commit. Run:
```bash
cd "/mnt/d/Shared With Desktop/AI/matter-engine-cpp" && git add MatterSurfaceLib/tests/part_asset_v2_tests.cpp && git commit -m "$(cat <<'EOF'
test: cover layout/corruption/v1-cutover guards for part v2 loader

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

### Task 9: Final full-suite verification

**Files:**
- (verification only)

- [ ] Run the full v2 suite from clean to confirm everything builds and passes. Run:
```bash
make -C "/mnt/d/Shared With Desktop/AI/matter-engine-cpp/MatterSurfaceLib/tests" clean && make -C "/mnt/d/Shared With Desktop/AI/matter-engine-cpp/MatterSurfaceLib/tests" run-partv2
```
Expected: clean build, then `All part_asset_v2 tests passed`, exit 0.

- [ ] Confirm the v1 suite still passes (the v2 changes are additive and must not regress v1). Run:
```bash
make -C "/mnt/d/Shared With Desktop/AI/matter-engine-cpp/MatterSurfaceLib/tests" run-part
```
Expected: `All part_asset tests passed`, exit 0.

- [ ] Confirm working tree is clean (all work committed). Run:
```bash
cd "/mnt/d/Shared With Desktop/AI/matter-engine-cpp" && git status --short
```
Expected: no modified/untracked source, header, Makefile, or test files from this plan.
