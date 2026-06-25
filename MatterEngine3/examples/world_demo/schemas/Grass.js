// Small voxel grass clump (leaf part): a few thin glass blades. Scattered in
// large numbers by the driver, all sharing one baked artifact (dedup demo).
class Grass extends Part {
  static params = { seed: 3 };
  build(p) {
    this.beginVoxels(0.2);
    this.fill(MAT.glass);
    this.box([0, 0.3, 0], [0.06, 0.3, 0.06]);
    this.box([0.15, 0.25, 0.05], [0.05, 0.25, 0.05]);
    this.box([-0.12, 0.2, -0.08], [0.05, 0.2, 0.05]);
    this.endVoxels();
  }
}
