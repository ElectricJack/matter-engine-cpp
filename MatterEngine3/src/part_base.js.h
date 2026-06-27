#pragma once
static const char* kPartBaseJS = R"JS(
globalThis.MAT = {
  bark: 14, leaf: 15, dirt: 16,
  grass: 2, stone: 8, stoneDark: 9, rock: 11,
  sand: 13, water: 7, metal: 3, glass: 4, light: 5,
};
globalThis.Part = class Part {
  build(p) {}
  pushMatrix()           { __dsl_pushMatrix(); }
  popMatrix()            { __dsl_popMatrix(); }
  translate(x,y,z)       { __dsl_translate(x,y,z); }
  rotateX(r)             { __dsl_rotateX(r); }
  rotateY(r)             { __dsl_rotateY(r); }
  rotateZ(r)             { __dsl_rotateZ(r); }
  scale(x,y,z)           { __dsl_scale(x,y,z); }
  applyMatrix(m)         { __dsl_applyMatrix(m); }
  fill(mat)              { __dsl_fill(mat); }
  beginVoxels(spacing)   { __dsl_beginVoxels(spacing); }
  endVoxels()            { __dsl_endVoxels(); }
  sphere(c,r)            { __dsl_sphere(c[0],c[1],c[2],r); }
  box(c,h)               { __dsl_box(c[0],c[1],c[2],h[0],h[1],h[2]); }
  union()                { __dsl_op(0); }
  difference()           { __dsl_op(1); }
  intersection()         { __dsl_op(2); }
  smoothing(k)           { __dsl_smoothing(k); }
  placeChild(module)     { __dsl_placeChild(module); }
};
)JS";
