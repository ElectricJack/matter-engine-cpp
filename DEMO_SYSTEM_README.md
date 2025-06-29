# Matter Engine 2 - Demo System Architecture

## Overview

The ParticleDynamicsExample has been refactored into a flexible demo system that supports multiple particle simulation demos. Each demo can be easily switched between at runtime and follows a standardized interface.

## Architecture

### Core Components

1. **DemoInterface** (`include/demo_interface.h`)
   - Abstract base class defining the interface all demos must implement
   - Provides standardized methods for initialization, cleanup, input handling, and rendering

2. **DemoManager** (`main.cpp`)
   - Manages multiple demos and handles switching between them
   - Provides shared particle system and camera management
   - Handles global input (demo switching, camera control, etc.)

3. **Individual Demos** (e.g., `src/solar_system_demo.cpp`)
   - Inherit from DemoInterface
   - Implement specific simulation scenarios
   - Handle their own specialized input and rendering

### Current Demos

#### 1. Solar System Demo (`solar_system_demo.cpp`)
- **Description**: Realistic solar system simulation with planets, asteroids, and orbital mechanics
- **Features**:
  - Central star with gravitational field
  - Multiple planets with different sizes and orbital distances
  - Asteroid belt with 200+ small particles
  - Realistic orbital velocity calculations
  - Dynamic simulation speed control

**Controls for Solar System Demo:**
- `SPACE` - Add asteroid with orbital velocity at camera position
- `A` - Create asteroid shower (50 asteroids in belt region)
- `P` - Add random planet with random orbital distance and properties
- `+/-` - Increase/Decrease simulation speed (0.001x to 1.0x)
- `D` - Toggle debug spatial visualization
- `N` - Toggle neighbor interaction lines
- `I` - Print performance statistics
- `T` - Reset profiling statistics
- `M` - Toggle rendering mode
- `R` - Reset solar system to initial state

### Global Controls (Available in All Demos)

- `TAB` - Switch to next demo
- `SHIFT+TAB` - Switch to previous demo
- `1-9` - Quick switch to demo by number
- `R` - Reset current demo
- `ESC` - Toggle mouse capture for camera control
- `WASD + Mouse` - Camera movement and look

## Demo Interface Specification

Each demo must implement these methods:

```cpp
class YourDemo : public DemoInterface {
public:
    // Get demo name for UI display
    const char* get_name() const override;
    
    // Get demo description
    const char* get_description() const override;
    
    // Initialize demo with shared particle system and camera
    void initialize(std::shared_ptr<ParticleSystem> particle_system, Camera& camera) override;
    
    // Clean up demo resources
    void cleanup() override;
    
    // Update demo logic each frame
    void update(float dt, Camera& camera) override;
    
    // Handle demo-specific input
    void handle_input(Camera& camera, std::shared_ptr<ParticleSystem> particle_system) override;
    
    // Render demo-specific UI elements
    void render_ui(int screen_width, int screen_height, std::shared_ptr<ParticleSystem> particle_system) override;
    
    // Render demo-specific 3D elements
    void render_3d(std::shared_ptr<ParticleSystem> particle_system) override;
    
    // Reset demo to initial state
    void reset(std::shared_ptr<ParticleSystem> particle_system, Camera& camera) override;
};
```

## Adding New Demos

To add a new demo to the system:

1. **Create Header File** (`include/your_demo.h`)
   ```cpp
   #pragma once
   #include "demo_interface.h"
   
   class YourDemo : public DemoInterface {
       // Implement all virtual methods
   };
   ```

2. **Create Implementation File** (`src/your_demo.cpp`)
   ```cpp
   #include "your_demo.h"
   // Implement all methods
   ```

3. **Register Demo in DemoManager** (`main.cpp`)
   ```cpp
   void register_demos() {
       demos_.push_back(std::make_unique<SolarSystemDemo>());
       demos_.push_back(std::make_unique<YourDemo>());  // Add here
   }
   ```

4. **Update Makefile**
   ```makefile
   SRC = main.cpp src/particle_system.cpp src/solar_system_demo.cpp src/your_demo.cpp src/object_allocator.c src/spatial_hash.c
   OBJ = $(OBJ_DIR)/main.o $(OBJ_DIR)/particle_system.o $(OBJ_DIR)/solar_system_demo.o $(OBJ_DIR)/your_demo.o $(OBJ_DIR)/object_allocator.o $(OBJ_DIR)/spatial_hash.o
   ```

## Planned Future Demos

The architecture is designed to support various simulation types:

- **Fluid Simulation Demo** - SPH (Smoothed Particle Hydrodynamics) fluid dynamics
- **Collision Stress Test** - High-performance collision detection and response
- **Galaxy Formation Demo** - Large-scale gravitational N-body simulation
- **Molecular Dynamics Demo** - Chemical simulation with bonds and reactions
- **Cloth/Soft Body Demo** - Deformable body physics
- **Electromagnetic Demo** - Charged particle interactions

## Building

Use the existing build system:

```bash
# Build for Windows (default)
make

# Build for Linux
make TARGET=linux

# Build for macOS  
make TARGET=macos

# Clean build
make clean
```

## Performance Features

The demo system maintains all performance optimizations from the original:

- **Structure of Arrays (SoA)** particle system for cache efficiency
- **Spatial hash** acceleration for neighbor finding
- **Profiling system** with detailed timing statistics
- **Multiple rendering modes** for performance comparison
- **Debug visualizations** for development and tuning

## UI System

Each demo provides its own UI while the DemoManager handles global elements:

- **Top Header**: Current demo info and description
- **Bottom Controls**: Global navigation and camera controls  
- **Right Panel**: Demo list with current selection highlighted
- **Demo Area**: Each demo controls the main UI space for its specific information

The UI is designed to be informative without cluttering the 3D view, with semi-transparent backgrounds for overlaid elements.

## Technical Notes

- Shared `ParticleSystem` instance across all demos for consistency
- Camera state is preserved when switching demos (unless demo resets it)
- Each demo manages its own particle types and configurations
- Memory management handled automatically through RAII and smart pointers
- All demos support the same profiling and debug visualization features

This architecture makes it easy to create focused demonstrations of different physics simulation techniques while maintaining a consistent user experience and shared codebase. 