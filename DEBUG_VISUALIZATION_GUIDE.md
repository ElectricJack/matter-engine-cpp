# Debug Visualization Guide

## Overview

The ParticleDynamicsExample now includes comprehensive debug visualization tools to help you visualize and tune the spatial hash optimization parameters. These tools show exactly how particles interact with each other and how the spatial partitioning affects performance.

## Debug Visualization Features

### 1. Spatial Cell Boundaries (Key: D)

**What it shows:**
- **Yellow wireframe cubes** around each particle showing the spatial hash cell it occupies
- Each cube represents a `SPATIAL_CELL_SIZE × SPATIAL_CELL_SIZE × SPATIAL_CELL_SIZE` grid cell

**How to use:**
- Press **D** to toggle spatial cell visualization
- Observe how particles are partitioned into grid cells
- Ideal tuning: cells should contain 2-8 particles on average

**What to look for:**
- ✅ **Good**: Particles distributed across multiple cells, some overlap between neighboring cells
- ⚠️ **Too large cells**: Many particles in single cells (reduce `SPATIAL_CELL_SIZE`)
- ⚠️ **Too small cells**: Most cells contain only 1 particle (increase `SPATIAL_CELL_SIZE`)

### 2. Gravity Influence Spheres (Key: D)

**What it shows:**
- **Colored transparent spheres** showing each particle's gravitational influence radius
- Color matches particle type (Blue=light, Green=medium, Red=heavy)
- Radius = `GRAVITY_BASE_RADIUS + (mass × MASS_RADIUS_MULTIPLIER)`

**How to use:**
- Press **D** to toggle gravity influence visualization
- Different particle types have different influence radii based on mass
- Spheres should overlap with nearby particles that interact

**What to look for:**
- ✅ **Good**: Spheres overlap with 5-15 nearby particles
- ⚠️ **Too small**: No overlaps, particles don't interact (increase `GRAVITY_BASE_RADIUS`)
- ⚠️ **Too large**: Spheres overlap with many distant particles (decrease `GRAVITY_BASE_RADIUS`)

### 3. Neighbor Connection Lines (Key: N)

**What it shows:**
- **Colored lines** connecting each particle to neighbors that affect its motion
- Line color matches the particle type being affected
- Line brightness indicates force strength (brighter = stronger gravitational pull)
- Thick lines for very close neighbors (high influence)

**How to use:**
- Press **N** to toggle neighbor connection lines
- Move camera around to see the interaction network
- Watch how connections change as particles move

**What to look for:**
- ✅ **Good**: 5-15 connections per particle, varying brightness
- ⚠️ **Too few connections**: Particles isolated (increase influence radius)
- ⚠️ **Too many connections**: Dense web of lines (decrease influence radius or increase cell size)

## Tuning Parameters

### Key Configuration Constants

```cpp
// In particle_system.h
static constexpr float SPATIAL_CELL_SIZE      = 2.0f;  // Size of spatial hash cells
static constexpr float GRAVITY_BASE_RADIUS    = 5.0f;  // Base gravity influence radius
static constexpr float MASS_RADIUS_MULTIPLIER = 0.5f;  // How much mass affects influence radius
static constexpr int   MAX_NEIGHBORS          = 16;    // Maximum neighbors to check per particle
```

### Tuning Process

1. **Start with default values** and run the simulation
2. **Add particles** with 'E' key (100 particles at once)
3. **Enable debug visualization** with 'D' key
4. **Enable neighbor lines** with 'N' key
5. **Observe the interaction patterns**:

#### Spatial Cell Size Tuning
- **If cells are too crowded** (>10 particles per cell): Increase `SPATIAL_CELL_SIZE`
- **If cells are mostly empty** (<2 particles per cell): Decrease `SPATIAL_CELL_SIZE`
- **Sweet spot**: 3-6 particles per active cell

#### Gravity Radius Tuning
- **If particles don't interact** (no connection lines): Increase `GRAVITY_BASE_RADIUS`
- **If too many connections** (>20 lines per particle): Decrease `GRAVITY_BASE_RADIUS`
- **Sweet spot**: 8-12 connections per particle in dense regions

#### Mass Multiplier Tuning
- **If particle mass doesn't matter**: Increase `MASS_RADIUS_MULTIPLIER`
- **If heavy particles dominate everything**: Decrease `MASS_RADIUS_MULTIPLIER`
- **Sweet spot**: Heavy particles have 1.5-2x the connections of light particles

### Performance Monitoring

**Watch these metrics while tuning:**
- **Physics step time** (shown in UI) should stay under 5ms for 100+ particles
- **Frame rate** should maintain 60 FPS with debug visualization enabled
- **Connection density** should balance realism vs performance

## Real-Time Tuning Workflow

1. **Launch simulation**: `./build/windows-native/particle_dynamics.exe`
2. **Add test particles**: Press 'E' several times to get 200-300 particles
3. **Enable all debug features**: Press 'D' and 'N'
4. **Observe patterns**:
   - Are yellow cubes well-distributed?
   - Do colored spheres have good overlap?
   - Are connection lines forming realistic interaction networks?
5. **Modify parameters** in code and rebuild to test different configurations

## Visual Interpretation

### Ideal Configuration
- **Yellow cubes**: Distributed grid with moderate particle density
- **Colored spheres**: Overlapping with nearby particles, clear boundaries
- **Connection lines**: Web-like network with varying brightness, no isolated particles

### Common Issues

**Problem**: Particles act like they're in separate universes
- **Symptoms**: No connection lines, particles don't interact
- **Solution**: Increase `GRAVITY_BASE_RADIUS` or decrease `SPATIAL_CELL_SIZE`

**Problem**: Simulation runs slowly with many particles
- **Symptoms**: Low FPS, high physics step time, dense web of connections
- **Solution**: Decrease `GRAVITY_BASE_RADIUS` or increase `SPATIAL_CELL_SIZE`

**Problem**: Heavy particles don't behave differently from light ones
- **Symptoms**: All influence spheres same size, similar connection counts
- **Solution**: Increase `MASS_RADIUS_MULTIPLIER`

## Advanced Usage

### Camera Controls for Debug Inspection
- **Mouse capture**: ESC to toggle mouse control
- **WASD**: Move camera around
- **Mouse**: Look around
- **Get close**: Move camera near particle clusters to see detail

### Performance Testing
1. **Baseline**: Start with few particles, note physics step time
2. **Scale test**: Add 100 particles (E key), measure performance impact
3. **Debug overhead**: Toggle debug features on/off to see rendering cost
4. **Stress test**: Keep adding particles until frame rate drops

### Comparing Configurations
1. **Note current parameters** and performance
2. **Modify one parameter** at a time
3. **Rebuild and test** with same particle count
4. **Compare**: Physics step time, visual behavior, interaction patterns

This debug visualization system makes spatial hash tuning much more intuitive - you can literally see how your parameter changes affect particle interactions! 