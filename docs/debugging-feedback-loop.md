# Debugging the GPU Raytracer: The Screenshot Feedback Loop

How the "raytracing renders black" and "adding particles breaks rendering" bugs
were found and fixed. The technique generalizes to any rendering / GPU bug where
you cannot single-step the pipeline and the only ground truth is the pixels.

## The core idea

A GPU fragment shader is opaque: you cannot set a breakpoint inside it, and a
wrong result is just a wrong-colored pixel. So you turn the renderer itself into
the instrument. Each iteration of the loop:

1. **Render a known scene through a debug shader path**, headless, to a PNG.
2. **Read the exact pixel values** back out (`python3` + PIL), don't just eyeball.
3. **Form one hypothesis** about what those pixels prove or rule out.
4. **Instrument one boundary** to gather evidence for that hypothesis.
5. **Repeat** until the pixels and the instrumentation agree on a root cause.

The pixels are the test oracle. Everything else is hypothesis.

## What made it reproducible and fast

- **Headless capture mode driven by env vars** (`MSL_CAPTURE`, `MSL_RATIO`,
  `MSL_RENDER_MODE`, `MSL_FRAMES`, `MSL_CAM`, `MSL_DEBUG_TRI`) so a single binary
  renders a deterministic scene to a file with no window interaction. Build the
  Linux ELF (`make WSL_LINUX=1`) and run under `DISPLAY=:0`.
- **A fixed camera and scene** so two captures are pixel-comparable. Without a
  deterministic scene you cannot tell a fix from noise.
- **Debug visualization shader paths** already in the shader (e.g.
  `debugTriangleTests` → triangle-test-count heatmap). A debug path that colors
  pixels by an internal quantity turns an invisible counter into something the
  capture loop can read.

## Reading pixels, not screenshots

Viewing the PNG tells you "it's black" or "one sphere is missing." Reading the
actual RGBA values tells you *which* black — a true (0,0,0) from a missed hit vs.
garbage from an unbound/oversized texture sampling out of range. Always pull the
numbers:

```python
from PIL import Image
im = Image.open("cap_rt.png")
print(im.size, im.getpixel((cx, cy)))
```

## Instrument at component boundaries (multi-layer systems)

The raytracer is CPU build → texture upload → GPU sample → shade. The bug could
live in any layer, so we logged what crosses each boundary:

- `[DBG REG]` — every BLAS registered on the CPU (handle, triangle count, hash).
- `[DBG TEX]` / `glGetIntegerv(GL_MAX_TEXTURE_SIZE)` — texture dimensions vs. the
  hardware cap at upload time.
- Debug shader path — what the GPU actually sampled.

This is what cracked it: `[DBG REG]` showed handles 1–17 *and* 18–34 with
identical triangle-count sequences but different content hashes — the CPU was
registering the same meshes twice and never freeing the first set. The texture
width (24544 cols) then exceeded `GL_MAX_TEXTURE_SIZE` (16384 on this WSL2/Mesa
box), so the upload silently failed and every sample returned zeros → black.

The lesson: the black pixels were a *symptom three layers downstream* of the
real fault (unbounded BLAS accumulation on the CPU). Tracing backward through the
boundaries, instead of guessing at the shader, is what found it.

## One root cause, two reported bugs

Both reported symptoms came from the same fault — BLAS entries were never
released across rebuilds, and mesh generation is not bit-deterministic so the
content-hash dedup couldn't reclaim them:

- **"Raytracing is black"** — the capture path does an init build + a
  `force_rebuild`, doubling entries past the texture-width cap in one shot.
- **"Adding particles breaks rendering"** — interactive `rebuild_dirty_cells`
  accumulates entries every frame until the count crosses the cap.

Finding the shared root cause is the payoff of investigating before fixing: one
fix (ref-counted BLAS lifecycle + release on re-mesh, plus 2D-tiled textures as
defense-in-depth) resolved both.

## Locking the fix in without a GPU

The leak lives entirely in CPU bookkeeping (`register_triangles` /
`release_blas`), so it can be regression-tested headlessly with no GL context
(`tests/blas_refcount_tests.cpp`). The key test re-meshes the "same" geometry 200
times with micro-perturbed coordinates (simulating non-deterministic generation)
and asserts the live BLAS count stays at 1. Before the fix it would have grown to
200 — exactly the accumulation that overflowed the texture.

A subtlety the test surfaced: `Tri` unions each `float3` with a 16-byte `__m128`,
and `triangles_equal`'s `memcmp` inspects the padding lane, so the test must
zero-init the struct for dedup to be deterministic. Reading pixels has a CPU
analogue: read the actual bytes, don't assume struct layout.

## Checklist for the next GPU bug

1. Can you render the failing case headlessly and deterministically? If not,
   build that first — it's the whole loop.
2. Read the pixel values, not the picture.
3. Add a debug shader path that colors by the suspect internal quantity.
4. Log what crosses each pipeline boundary (CPU build → upload → sample → shade)
   and find which boundary first shows bad data.
5. Trace backward from there to the source; fix the source, not the symptom.
6. Check hardware limits (`GL_MAX_TEXTURE_SIZE`) — silent upload failures look
   exactly like logic bugs.
7. If the fault is CPU bookkeeping, write a headless regression test that
   reproduces the accumulation, not just the end state.
