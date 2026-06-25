#pragma once
// Evaluated into every fresh context before user source. Defines the Part base
// and a registry the host reads to discover the authored subclass.
static const char* kPartBaseJS = R"JS(
globalThis.__parts = [];
globalThis.Part = class Part {
  build(p) {}
  static __register(cls) { globalThis.__parts.push(cls); }
};
// Capture subclasses at definition time via a static initializer pattern:
// user classes call super-less; the host enumerates globalThis for class ctors
// extending Part after eval (see host). This stub keeps Part defined.
)JS";
