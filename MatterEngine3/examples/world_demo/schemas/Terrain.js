// Blocky voxel terrain tile (leaf part). A dirt ground slab with a couple of
// stone steps so flattened tiles read as relief rather than a flat plane.
// One tile spans 8x8 world units (half-extent 4); the driver lays a grid of them.
class Terrain extends Part {
  static params = { seed: 1 };
  build(p) {
    this.beginVoxels(0.5);
    this.fill(MAT.dirt);
    this.box([0, 0, 0], [4, 0.5, 4]);          // ground slab (8 x 1 x 8)
    this.fill(MAT.stone);
    this.box([2, 0.75, 2], [1.5, 0.75, 1.5]);  // raised mound (union)
    this.box([-2.5, 0.5, -1.5], [1, 0.5, 1]);  // smaller step (union)
    this.endVoxels();
  }
}
