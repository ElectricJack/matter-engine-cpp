# Performance Profiling Guide

## Overview

The ParticleDynamicsExample now includes comprehensive performance profiling capabilities to help you identify bottlenecks and optimize the spatial hash particle system. The profiling system tracks timing for all major subsystems and provides detailed statistics.

## Profiling Features

### ✅ **Automatic Frame Profiling**
- **Frame timing** - Total time per frame and FPS calculation
- **Section breakdown** - Detailed timing for each major subsystem
- **Statistical analysis** - Min/max/average times and percentages
- **Real-time display** - Live profiling metrics in the UI

### ✅ **Detailed Section Tracking**
The profiler automatically tracks these key sections:
- **Total Physics Update** - Complete physics simulation step
- **Populate Spatial Hash** - Building the spatial acceleration structure
  - Clear Spatial Hash
  - Clear Particle Refs  
  - Insert Particles
- **Apply Gravitational Forces** - All force calculations
  - Black Hole Forces
  - Particle-Particle Forces (spatial optimized)
- **Collision Detection** - Collision detection and merging
  - Find Collision Pairs
  - Process Collisions
- **Integrate Particles** - Position/velocity integration
- **Particle Rendering** - Visual rendering of particles
- **Debug Visualization** - Debug drawing (when enabled)

## Using the Profiling System

### Real-Time Profiling Display

The UI shows live profiling metrics:
```
Physics step: 2.34 ms
Profiling (averaged):
  Spatial Hash: 0.45 ms
  Gravity Forces: 1.22 ms
  Collision Det: 0.31 ms  
  Integration: 0.36 ms
```

### Interactive Controls

- **P** - Print detailed profiling statistics to console
- **T** - Reset profiling statistics (start fresh measurement)

### Detailed Console Output

Press **P** to see comprehensive profiling data:
```
=== Performance Statistics ===
Frame: 16.67 ms (60.0 FPS)
Frames: 1200

Section Breakdown:
Section                   Avg(ms)  Min(ms)  Max(ms)  Total(ms)  Calls    %
-------                   -------  -------  -------  --------  -----   --
Total Physics Update        2.34     1.89     4.12    2808.5   1200   14.0%
Populate Spatial Hash       0.45     0.32     0.89     540.2   1200    2.7%
Total Gravitational Forces  1.22     0.95     2.31    1464.8   1200    7.3%
Particle-Particle Forces    0.98     0.72     2.01    1176.4   1200    5.9%
Black Hole Forces           0.24     0.18     0.42     288.6   1200    1.4%
Collision Detection         0.31     0.22     0.55     372.1   1200    1.9%
Integrate Particles         0.36     0.28     0.48     432.4   1200    2.2%
```

## Performance Analysis Workflow

### 1. Baseline Measurement
```bash
# Start the simulation
./build/windows-native/particle_dynamics.exe

# Reset profiling stats to start fresh
Press 'T'

# Let it run for ~30 seconds to get stable averages
# Check real-time metrics in UI
```

### 2. Performance Testing
```bash
# Add particles to stress test
Press 'E' multiple times (100 particles each)

# Monitor real-time profiling display
# Watch for sections that spike in time
# Note which sections dominate total time
```

### 3. Detailed Analysis
```bash
# Print comprehensive stats
Press 'P'

# Look for:
# - Sections with highest average time
# - Sections with highest percentage of frame time
# - Large differences between min/max (inconsistent performance)
# - Sections that scale poorly with particle count
```

## Interpreting Profiling Results

### Key Metrics to Watch

**Frame Rate Impact:**
- **< 16.67ms frame time** = 60+ FPS (excellent)
- **16.67-33.33ms frame time** = 30-60 FPS (good)
- **> 33.33ms frame time** = < 30 FPS (needs optimization)

**Section Analysis:**
- **Spatial Hash** should be < 20% of total physics time
- **Gravity Forces** typically dominates (40-60% of physics time)
- **Collision Detection** should be < 10% with few collisions
- **Integration** should be minimal (< 15% of physics time)

### Common Performance Patterns

**Well-Optimized System:**
```
Total Physics Update: 2.1ms (12.6% of frame)
├── Spatial Hash: 0.3ms (14% of physics)
├── Gravity Forces: 1.2ms (57% of physics)  
├── Collision Det: 0.2ms (10% of physics)
└── Integration: 0.4ms (19% of physics)
```

**Spatial Hash Bottleneck:**
```
Total Physics Update: 5.2ms (31.2% of frame)
├── Spatial Hash: 2.1ms (40% of physics) ← PROBLEM!
├── Gravity Forces: 2.3ms (44% of physics)
├── Collision Det: 0.4ms (8% of physics)
└── Integration: 0.4ms (8% of physics)
```

**Force Calculation Bottleneck:**
```
Total Physics Update: 8.1ms (48.6% of frame)
├── Spatial Hash: 0.4ms (5% of physics)
├── Gravity Forces: 6.8ms (84% of physics) ← PROBLEM!
├── Collision Det: 0.5ms (6% of physics)
└── Integration: 0.4ms (5% of physics)
```

## Optimization Strategies

### If Spatial Hash is Slow (> 25% of physics time):
- **Increase `SPATIAL_CELL_SIZE`** - Fewer, larger cells
- **Reduce particle count** - Test with fewer particles
- **Check `sh_clear()` performance** - May need faster clearing

### If Gravity Forces are Slow (> 70% of physics time):
- **Decrease `GRAVITY_BASE_RADIUS`** - Fewer neighbor queries
- **Reduce `MAX_NEIGHBORS`** - Limit neighbor search
- **Optimize force calculation** - Use lookup tables for sqrt()

### If Collision Detection is Slow (> 15% of physics time):
- **Reduce `COLLISION_DISTANCE`** - Smaller collision radius
- **Limit collision processing** - Process fewer collisions per frame
- **Optimize collision response** - Simplify particle merging

### If Debug Visualization is Slow:
- **Disable neighbor lines** when not tuning (Press 'N')
- **Disable spatial visualization** when not needed (Press 'D')
- **Reduce debug geometry complexity** - Fewer sphere segments

## Profiling Different Scenarios

### Particle Count Scaling
```bash
# Test 50 particles
Reset simulation ('R'), measure baseline

# Test 100 particles  
Add particles ('E'), compare times

# Test 200 particles
Add more particles ('E'), note scaling behavior

# Expected: Linear scaling for well-optimized spatial hash
# Bad: Quadratic scaling indicates spatial optimization failure
```

### Parameter Tuning Impact
```bash
# Before changing parameters
Press 'P' to record baseline stats

# Modify parameters in code and rebuild
# After changes
Press 'T' to reset, then 'P' to compare

# Look for improvements in bottleneck sections
```

## Performance Targets

### Excellent Performance (Production Ready)
- **Total Physics**: < 3ms for 200 particles
- **60+ FPS** sustained with debug features disabled
- **Linear scaling** with particle count

### Good Performance (Interactive)
- **Total Physics**: < 5ms for 200 particles  
- **30+ FPS** sustained
- **Near-linear scaling** up to 500 particles

### Needs Optimization
- **Total Physics**: > 8ms for 200 particles
- **< 30 FPS** with moderate particle counts
- **Quadratic scaling** behavior observed

## Advanced Profiling

### Custom Section Profiling
```cpp
// Add custom profiling to any code section
{
    PROFILE_SECTION("My Custom Section");
    // Your code here
}
```

### Frame-by-Frame Analysis
```cpp
// Profile individual frames in debug mode
PROFILE_FRAME_BEGIN();
// ... frame code ...
PROFILE_FRAME_END();
```

The profiling system provides everything needed to identify and resolve performance bottlenecks in the spatial hash particle system. Use it to validate optimizations and ensure your particle system scales efficiently! 