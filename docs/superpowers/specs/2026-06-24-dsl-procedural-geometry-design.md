# DSL & Procedural Geometry Interface — Design

**Status:** Approved for planning (2026-06-24)
**Context:** Defines the authoring surface (the script-facing DSL) for the procedural
part system in `2026-06-24-procedural-part-authoring-design.md`. The prototype's
Processing-style `BaseBuildScript` (`D:\Dev\MatterEngine2\VoxelSource\ScriptingEngine\`)
is the starting point; this doc decides what survives under the new content-addressed,
baked-artifact architecture and adds the two things the prototype stubbed or lacked:
**first-class lattice structures** and **real material filling**.

## Goal

A simple, stateful, Processing-style imperative DSL that authors a **part** — a baked
artifact (geometry + child-instance records) keyed by content hash. The DSL must:

- Keep the prototype's immediate-mode ergonomics (transform stack, current-material
  cursor, `begin*/end*` sessions).
- Emit geometry three ways: direct triangles, SDF/CSG voxels, and **structured lattices**.
- Fill structures with **real materials** (driving the existing `MaterialDef` registry,
  not stubbed colors).
- Treat a lattice as a **mutable, queryable in-build grid** so scripts can run
  deposition (snow) and erosion-style passes, read positions/state, and mutate in place.

## Why the stateful model survives the new architecture

Under content-addressed baking, `build()` is a **pure function of
`(script_source ⊕ params ⊕ sorted(child_resolved_hashes))`**. All the mutable cursor
state — transform stack, current material, the open shape/voxel/lattice session, the
in-build lattice grid — is **transient scratch state** that exists only during one bake
and produces a deterministic artifact. The cache neither sees nor cares about it.

So the Processing-style imperative feel is fully compatible with "build once, cache by
hash." What changes versus the prototype is *what the state holds* and *what `end*`
emits into*, not the imperative shape of the API:

- `fill` stops carrying a *color* and carries a **material id** (registry-driven).
- `stroke` color is removed; 1D primitives instead carry a **skinning radius**.
- A third emission mode (**lattice**) joins shapes and voxels.
- `end*` flushes into a **baked artifact**, not an implicit single buffer.

---

## DSL surface

### Transform stack (survives unchanged)

`pushMatrix()` / `popMatrix()` / `translate()` / `rotateX|Y|Z()` / `scale()` /
`applyMatrix()` / `lookAt()`. Standard Processing semantics; the current matrix is
applied to every emitted primitive, slot, and child instance.

### Material cursor

- `fill(material)` — sets the current material id into the existing `MaterialDef`
  registry. Every primitive, slot, and shape emitted afterward inherits it.
- **Blend behavior is registry-driven.** Whether two adjacent fills metaball-blend or
  stay separate is decided by each material's `mergeGroup` in the registry — the author
  picks *what* the material is, the registry decides *how* it blends. `fill()` is a
  single argument in the common case. (A future optional per-fill group override is
  additive and out of scope for v1.)

### Three peer emission sessions (mutually exclusive at any instant)

You are in exactly one session at a time. Opening a session while another is active is
an authoring error. The three sessions hit different backend paths and stay
independently testable.

**1. Direct-triangle session** — `beginShape(type)` / `vertex(...)` / `endShape()`,
plus `line(a, b)`.
- 1D primitives (`line`, `LINES`-type shapes) carry a **skinning radius** (e.g.
  `lineThickness(r)` / a radius arg) — *not* a stroke color. Inside emission this tubes
  the 1D primitive into solid geometry (stepped spheres along the segment, radius
  lerpable), which is how the L-system trunk/branch workflow stays concise.

**2. Voxel/SDF-CSG session** — `beginVoxels(spacing)` / `endVoxels()`.
- Primitives: `box(...)`, `sphere(...)`.
- CSG: `union()` / `difference()` / `intersection()`.
- `smoothing(...)`.

**3. Lattice session** — `beginLattice(type, spacing)` / `endLattice()`.
- `type` selects a `Lattice` implementation (`grid` for v1; hex/diamond become new
  `Lattice` impls later). `spacing` is the **structural authoring granularity** — how
  coarse/fine the slots are — *not* a rendering-detail knob (see Detail below).
- The lattice is a **mutable, queryable grid** that lives for the whole session (see
  next section).

### Sequential sessions merge into one artifact

A single part may open multiple sessions **in sequence** (e.g. a lattice pass, then a
shape pass). Each `end*` flushes its result into the part's single baked artifact.
Overlapping volumes resolve at **surface time** via the material/`mergeGroup` rules —
the DSL does not pre-merge; it accumulates emissions and the surfacer applies blend/carve
semantics. This is required, not optional: real parts mix structured fills with direct
detail geometry.

### Child instancing & scatter

- `instance(childPart, variation)` — records a **child-instance entry** (child part
  reference + the current transform-stack matrix) into the baked artifact. No geometry
  is inlined; the child stays a separate BLAS instanced at render time. This is a direct
  fit with the existing BLAS-instancing backend and survives essentially unchanged from
  the prototype's `AddInstance`.
- **Scatter is representation-honest:**
  - Lattice-native: `filledSlots()` / `slotsMatching(predicate)` — the grid already *is*
    a set of discrete positions, so scatter is just sampling filled slots. No hidden
    point-in-volume machinery.
  - Mesh surfaces: a separate `pointsOnSurface(n, seed)` call for direct geometry.

---

## The lattice as a mutable in-build data structure

This is the substantive new capability. A lattice session exposes the grid for
**write, read, and in-place mutation** across multiple passes — enabling snow
deposition, erosion, and similar cellular algorithms.

### Write

- `slot(x, y, z)` — fill one slot with the current material.
- Primitive fills — `fillLine(a, b)`, `fillBox(min, max)`, `fillSphere(center, r)` — set
  all slots in the volume to the current material.
- **Mesh-stencil fill** — fill slots inside/near a previously generated mesh, using that
  mesh as a stencil for selective filling.

### Read

- `get(x, y, z)` → material id or empty.
- `isFilled(x, y, z)`.
- Neighbor sampling and slot → world-position lookup (so scripts can reason about
  geometry, e.g. "is this an exposed top face").

### Mutate in place

- A **small** set of fast built-in C++ operators for v1 (whole-grid passes stay in C++):
  - `deposit(material, fromDir, amount)` — settle material onto exposed faces (snow).
  - `erode(iterations, threshold)` — remove/relocate filled slots (erosion).
  - `dilate(iterations)` — grow filled regions.
  - *(Kept deliberately small for v1; more operators are additive.)*
- **`forEach((x, y, z, material) => { ... })` escape hatch** — author-written per-slot
  rules in JS for anything the built-ins don't cover. This crosses the JS↔C++ boundary
  per slot, so it is slow on large grids — an **opt-in** cost the author takes
  knowingly, and one the content-address cache hides entirely for static parts.

### Performance note

A large grid (e.g. 256³ ≈ 16M slots) × N script-driven passes can make a bake slow. Two
mitigations are built into the architecture: (1) the fast built-in operators keep the
common snow/erosion cases entirely in C++; (2) bake-once + content-addressed caching
means the cost is paid once per unique `(source ⊕ params ⊕ child-hashes)` and never
again. Heavy `forEach` use on huge grids is the author's call.

---

## Detail / mesh resolution — NOT a DSL concept

Mesh-rendering resolution is a **world-scale engine constant, uniform everywhere**, and
is **never authored**. This intentionally decouples two things the prototype conflated:

- **Lattice spacing** = the author's *structural* granularity (where slots are). An
  authoring choice, per session.
- **Mesh detail** = the world's *rendering* resolution. A global engine parameter the
  surfacer applies uniformly to every part it meshes.

The motivation is **consistent detail across the world + predictable performance**: the
author cannot request locally-finer surfacing, and that uniformity is the goal, not a
limitation. This supersedes the tiered-surface-lattice spec's notion of
author-configurable `detail_size`/`tier` for the *authoring path* — tier remains a
backend/world-scale concern, not a DSL knob.

---

## Backend mapping

| DSL concept | Existing backend |
|---|---|
| `fill(material)` + blend | `MaterialDef` registry, `mergeGroup`, material-aware carving |
| Direct-triangle session | direct mesh emission → BLAS |
| Voxel/CSG session | SDF CSG → dual-contour/marching surfacer → BLAS |
| Lattice session | `Lattice` / `GridLattice` (slot coords + neighbor offsets) |
| `instance(child, variation)` | child-instance record → TLAS BLAS instance |
| Baked artifact | `part_asset` (.part serialization, FNV-1a content hash) |
| Detail (world-uniform) | surfacer resolution constant |

---

## Goals / Non-goals

**v1 goals**
- Transform stack, current-material cursor, three peer emission sessions, sequential
  session merge.
- Direct-triangle + voxel/CSG sessions matching prototype capability (skinning radius on
  1D primitives; no stroke color).
- `fill(material)` driving the real registry with registry-driven `mergeGroup`.
- Lattice session as a mutable/queryable grid: imperative + primitive fills, mesh-stencil
  fill, read/neighbor/position queries, small built-in mutation operator set
  (`deposit`/`erode`/`dilate`) + `forEach` escape hatch.
- Lattice-native scatter (`filledSlots`/`slotsMatching`) + mesh `pointsOnSurface`.
- `instance(childPart, variation)` → child-instance records.
- World-uniform detail (no DSL detail knob).

**Explicit non-goals for v1 (deferred)**
- Per-fill `mergeGroup` override (additive optional arg later).
- Author-configurable `detail_size`/`tier` (world-uniform for now).
- Hex/diamond lattices (new `Lattice` impls later; only `grid` in v1).
- Predicate/field-function fills as a first-class form (expressible via `forEach` / loops).
- A large library of mutation operators beyond the v1 trio.
- Mesh-modifier stack / imposter-type DSL state (prototype stubs; not carried over).

---

## Open questions (resolve during planning)

- Exact signatures/units for the skinning radius on 1D primitives (constant vs. per-end
  lerp like the prototype's stepped-sphere width).
- Mesh-stencil fill semantics: inside-test only, or inside-plus-shell? Tolerance band?
- `deposit`/`erode`/`dilate` parameterization details (direction encoding, thresholds,
  whether erode relocates vs. deletes material).
- How sequential-session overlaps resolve when materials are in *different* mergeGroups
  across sessions (carve vs. coexist) — likely falls out of the material-aware-surfacing
  cross-group rules, to confirm against that spec.
- Error/diagnostic model for session misuse (opening a session inside another, emitting
  outside a session).
