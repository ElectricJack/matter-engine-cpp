// Voxel tree (leaf part): a dirt trunk topped by a glass canopy stand-in.
// Imports the shared script library's seeded RNG (exercises SP-7 module
// resolution + source-fold) to vary the canopy radius from the part's seed.
import { rng } from 'shared-lib/rng';

class Tree extends Part {
  static params = { seed: 7 };
  build(p) {
    const r = rng(p.seed);
    const canopy = 1.2 + r.range(0.0, 0.8);   // seed-driven canopy radius
    this.beginVoxels(0.4);
    this.fill(MAT.dirt);
    this.box([0, 1.0, 0], [0.3, 1.0, 0.3]);   // trunk
    this.fill(MAT.glass);
    this.sphere([0, 2.4, 0], canopy);          // canopy (union)
    this.endVoxels();
  }
}
