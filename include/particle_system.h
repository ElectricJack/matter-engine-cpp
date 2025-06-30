#pragma once

extern "C" {
    #include "raylib.h"
    #include "../../SpatialQueryLib/include/spatial_hash.h"
}
#include "raymath.h"
#include "profiler.hpp"

// Lighting system
#define RLIGHTS_IMPLEMENTATION
#include "rlights.h"

#include <vector>
#include <memory>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

// Material types for the sandbox simulation
enum class MaterialType : uint32_t {
    Water = 0,
    Oxygen,
    Hydrogen,
    Carbon,
    Rock,
    Wood,
    Plant,
    Iron,
    Copper,
    Gold,
    Oil,
    Uranium,
    IronOxide,  // For rust reactions
    Plasma,     // For electrical breakdown
    COUNT       // Total number of materials
};

// Hash function for MaterialType pairs (needed for adhesion matrix)
namespace std {
    template<>
    struct hash<std::pair<MaterialType, MaterialType>> {
        size_t operator()(const std::pair<MaterialType, MaterialType>& p) const {
            return std::hash<uint32_t>()(static_cast<uint32_t>(p.first)) ^ 
                   (std::hash<uint32_t>()(static_cast<uint32_t>(p.second)) << 1);
        }
    };
}

// Phase states
enum class PhaseState : uint8_t {
    Solid = 0,
    Liquid,
    Gas,
    Plasma
};

// Material properties structure
struct MaterialProperties {
    const char* name;
    float density;              // kg/m³
    float heat_capacity;        // J/kg·K
    float thermal_conductivity; // W/m·K
    float melt_energy;          // J/kg
    float vapor_energy;         // J/kg
    float emissivity;           // Stefan-Boltzmann coefficient
    float electrical_conductivity; // S/m
    float permittivity;         // Relative permittivity
    float spark_threshold;      // V/m for breakdown
    float melt_point;           // °C
    float boil_point;           // °C
    float cohesion;             // Self-stickiness (0-1)
    Color base_color;           // Base color for rendering
    PhaseState default_phase;
    
    MaterialProperties(const char* n = "Unknown", float d = 1000.0f, float hc = 1000.0f,
                      float tc = 1.0f, float me = 100000.0f, float ve = 1000000.0f,
                      float e = 0.5f, float ec = 1e-6f, float p = 1.0f, float st = 3e6f,
                      float mp = 0.0f, float bp = 100.0f, float c = 0.5f,
                      Color color = WHITE, PhaseState phase = PhaseState::Solid)
        : name(n), density(d), heat_capacity(hc), thermal_conductivity(tc),
          melt_energy(me), vapor_energy(ve), emissivity(e),
          electrical_conductivity(ec), permittivity(p), spark_threshold(st),
          melt_point(mp), boil_point(bp), cohesion(c), base_color(color),
          default_phase(phase) {}
};

// Chemical reaction definition
struct ChemicalReaction {
    std::unordered_map<MaterialType, int> reactants;
    std::unordered_map<MaterialType, int> products;
    float activation_temperature;  // °C
    float energy_change;          // J per reaction
    float probability;            // Reaction probability per frame
    
    ChemicalReaction(float temp = 100.0f, float energy = 0.0f, float prob = 0.01f)
        : activation_temperature(temp), energy_change(energy), probability(prob) {}
};

// Particle bond structure
struct ParticleBond {
    uint32_t particle_index;
    float strength;
    float rest_length;
    
    ParticleBond(uint32_t idx = 0, float str = 1.0f, float len = 1.0f)
        : particle_index(idx), strength(str), rest_length(len) {}
};

// Particle type definition (shared properties for all particles of this type)
struct ParticleType {
    float radius;
    MaterialType material;
    float mass;
    Color color;
    
    ParticleType(float r = 0.5f, MaterialType mat = MaterialType::Water, float m = 1.0f, Color c = WHITE) 
        : radius(r), material(mat), mass(m), color(c) {}
};

struct BlackHole {
    Vector3 position;
    float mass;
    float radius;
    Color color;
    
    BlackHole() : position({0, 0, 0}), mass(100.0f), radius(2.0f), color(BLACK) {}
};

// Simple particle reference for spatial hash lookups
struct ParticleRef {
    uint32_t particle_index;
    
    ParticleRef(uint32_t idx = 0) : particle_index(idx) {}
};

class ParticleSystem {
public:
    ParticleSystem();
    ~ParticleSystem();
    
    // System management
    void initialize();
    void cleanup();
    void reset();
    
    // Particle management
    uint32_t create_particle_type(float radius, MaterialType material, float mass, Color color);
    void add_particle(uint32_t type_id, const Vector3& position, const Vector3& velocity, 
                     float temperature = 20.0f, float charge = 0.0f);
    void remove_particle(uint32_t particle_index);
    
    // Simulation
    void update(float dt);
    void render();
    void set_camera_position(const Vector3& camera_pos);
    
    // Material physics simulation
    void update_thermal_simulation(float dt);
    void update_electrical_simulation(float dt);
    void update_chemical_reactions(float dt);
    void update_bonding_system(float dt);
    
    // Debug visualization
    void render_debug_spatial_info();
    void set_debug_spatial_visualization(bool enabled) { debug_spatial_vis_ = enabled; }
    bool get_debug_spatial_visualization() const { return debug_spatial_vis_; }
    void set_debug_neighbor_lines(bool enabled) { debug_neighbor_lines_ = enabled; }
    bool get_debug_neighbor_lines() const { return debug_neighbor_lines_; }
    void set_debug_thermal_visualization(bool enabled) { debug_thermal_vis_ = enabled; }
    bool get_debug_thermal_visualization() const { return debug_thermal_vis_; }
    void set_debug_electrical_visualization(bool enabled) { debug_electrical_vis_ = enabled; }
    bool get_debug_electrical_visualization() const { return debug_electrical_vis_; }
    void set_debug_bonds_visualization(bool enabled) { debug_bonds_vis_ = enabled; }
    bool get_debug_bonds_visualization() const { return debug_bonds_vis_; }
    
    // Rendering mode control
    void set_instanced_rendering(bool enabled) { use_instanced_rendering_ = enabled; }
    bool get_instanced_rendering() const { return use_instanced_rendering_; }
    void cycle_rendering_mode();
    const char* get_rendering_mode_name() const;
    
    // Physics simulation toggles
    void set_gravity_simulation(bool enabled) { gravity_simulation_ = enabled; }
    bool get_gravity_simulation() const { return gravity_simulation_; }
    void set_thermal_simulation(bool enabled) { thermal_simulation_ = enabled; }
    bool get_thermal_simulation() const { return thermal_simulation_; }
    void set_electrical_simulation(bool enabled) { electrical_simulation_ = enabled; }
    bool get_electrical_simulation() const { return electrical_simulation_; }
    void set_chemical_simulation(bool enabled) { chemical_simulation_ = enabled; }
    bool get_chemical_simulation() const { return chemical_simulation_; }
    void set_bonding_simulation(bool enabled) { bonding_simulation_ = enabled; }
    bool get_bonding_simulation() const { return bonding_simulation_; }
    
    // Black hole control (for solar system demo)
    void set_black_hole_enabled(bool enabled) { black_hole_enabled_ = enabled; }
    bool get_black_hole_enabled() const { return black_hole_enabled_; }
    
    // Gravitational forces (legacy)
    void apply_gravitational_forces(float dt);
    void apply_black_hole_forces(float dt);
    void apply_particle_particle_forces_spatial(float dt);
    void handle_particle_collisions_spatial();
    
    // Material properties access
    const MaterialProperties& get_material_properties(MaterialType material) const;
    static const std::unordered_map<std::pair<MaterialType, MaterialType>, float, 
                                   std::hash<std::pair<MaterialType, MaterialType>>>& get_adhesion_matrix();
    
    // Info
    int get_particle_count() const;
    float get_physics_time_ms() const;
    float get_average_temperature() const;
    float get_total_electrical_energy() const;
    int get_active_reactions_count() const;
    int get_total_bonds_count() const;
    
    // Profiling interface
    void print_profiling_stats() const;
    void reset_profiling_stats();
    double get_profiling_section_time(const std::string& section) const;

private:
    // Instanced rendering data
    struct ParticleInstanceData {
        Vector3 position;
        float radius;
        Color color;
        float _padding; // Align to 16 bytes
    };
    
    // Instanced rendering resources
    Mesh                              sphere_mesh_;
    Material                          particle_material_;
    std::vector<ParticleInstanceData> instance_buffer_;
    bool                              instanced_rendering_initialized_;
    
    // Instanced rendering methods
    void initialize_instanced_rendering();
    void cleanup_instanced_rendering();
    void render_particles_instanced();
    void collect_instance_data();
    
    // Particle type registry
    std::vector<ParticleType> particle_types_;
    
    // Structure of Arrays for particles
    std::vector<float>     pos_x_, pos_y_, pos_z_;
    std::vector<float>     vel_x_, vel_y_, vel_z_;
    std::vector<float>     temperature_;
    std::vector<float>     charge_;              // Electrical charge
    std::vector<float>     voltage_;             // Electrical potential
    std::vector<uint32_t>  type_id_;
    std::vector<PhaseState> phase_state_;        // Current phase state
    std::vector<bool>      active_;              // Track which particles are active
    std::vector<uint32_t>  free_indices_;        // Free list for removed particles
    std::vector<std::vector<ParticleBond>> bonds_; // Bonds for each particle
    
    // Black hole
    BlackHole black_hole_;
    
    // Spatial hash for efficient neighbor queries
    SpatialHash* spatial_hash_;
    std::vector<ParticleRef> particle_refs_;  // Pool of particle references for reuse
    
    // Material properties lookup table
    static std::vector<MaterialProperties> material_properties_;
    static std::unordered_map<std::pair<MaterialType, MaterialType>, float,
                             std::hash<std::pair<MaterialType, MaterialType>>> adhesion_matrix_;
    static std::vector<ChemicalReaction> chemical_reactions_;
    static bool static_data_initialized_;
    
    // Initialize static material data
    static void initialize_material_properties();
    static void initialize_adhesion_matrix();
    static void initialize_chemical_reactions();
    
    // Debug visualization
    bool debug_spatial_vis_ = false;
    bool debug_neighbor_lines_ = false;
    bool debug_thermal_vis_ = false;
    bool debug_electrical_vis_ = false;
    bool debug_bonds_vis_ = false;
    
    // Physics simulation toggles
    bool gravity_simulation_ = true;
    bool thermal_simulation_ = true;
    bool electrical_simulation_ = true;
    bool chemical_simulation_ = true;
    bool bonding_simulation_ = true;
    bool black_hole_enabled_ = false;  // Black hole disabled by default
    
    // Performance tracking
    float physics_time_ms_;
    
    // Physics parameters
    static constexpr float GRAVITATIONAL_CONSTANT = 50.0f;   // Scaled for simulation
    static constexpr float MIN_DISTANCE           = 0.1f;    // Prevent singularities
    static constexpr float MAX_DISTANCE           = 200.0f;  // Simulation bounds
    static constexpr float DAMPING                = 1.0f;    // No damping for stable orbits
    static constexpr float COLLISION_DISTANCE     = 0.15f;   // Distance for particle merging
    
    // Spatial optimization parameters
    static constexpr float SPATIAL_CELL_SIZE      = 1.0f;    // Size of spatial hash cells
    static constexpr float GRAVITY_BASE_RADIUS    = 5.0f;    // Base gravity influence radius
    static constexpr float MASS_RADIUS_MULTIPLIER = 100.0f;  // How much mass affects influence radius
    static constexpr int   MAX_NEIGHBORS          = 64;      // Maximum neighbors to check per particle
    
    // Material physics parameters
    static constexpr float THERMAL_DIFFUSION_RATE = 0.1f;    // Heat transfer rate
    static constexpr float ELECTRICAL_RESISTANCE  = 0.01f;   // Base electrical resistance
    static constexpr float BOND_FORMATION_DISTANCE = 0.5f;   // Distance for bond formation
    static constexpr float BOND_BREAK_FORCE       = 10.0f;   // Force required to break bonds
    static constexpr float STEFAN_BOLTZMANN       = 5.67e-8f; // Stefan-Boltzmann constant
    
    // Rendering mode control
    bool use_instanced_rendering_ = true;
    
    // Raylib lighting system
    Shader lighting_shader_;
    bool lighting_initialized_ = false;
    Light lights_[MAX_LIGHTS];
    
    // Sphere mesh for proper lighting
    Mesh lighting_sphere_mesh_;
    Model lighting_sphere_model_;
    bool sphere_model_initialized_ = false;
    Vector3 camera_position_ = {0.0f, 15.0f, 30.0f}; // Current camera position for lighting
    
    // Internal methods
    void integrate_particles(float dt);
    void check_bounds();
    
    // Spatial hash methods
    void populate_spatial_hash();
    float calculate_gravity_radius(float mass) const;
    float calculate_collision_radius(float radius) const;
    
    // Material physics methods
    void apply_thermal_conduction(float dt);
    void apply_phase_changes(float dt);
    void apply_radiative_cooling(float dt);
    void apply_electrical_conduction(float dt);
    void apply_joule_heating(float dt);
    void check_dielectric_breakdown();
    void process_chemical_reactions(float dt);
    void update_particle_bonds(float dt);
    void apply_bond_forces(float dt);
    
    // Helper methods
    Color get_material_color(uint32_t particle_index) const;
    float calculate_thermal_conductivity_between(uint32_t p1, uint32_t p2) const;
    float calculate_electrical_conductivity_between(uint32_t p1, uint32_t p2) const;
    bool can_react(const std::unordered_map<MaterialType, int>& reactants, 
                   const std::vector<uint32_t>& nearby_particles) const;
    void consume_reactants(const std::unordered_map<MaterialType, int>& reactants,
                          const std::vector<uint32_t>& nearby_particles);
    void spawn_products(const std::unordered_map<MaterialType, int>& products,
                       const Vector3& reaction_center, float energy_change);
    
    // Debug visualization helpers
    void draw_spatial_cell_boundaries(float x, float y, float z);
    void draw_gravity_influence_sphere(float x, float y, float z, float radius, Color color);
    void draw_neighbor_connections();
    void draw_thermal_visualization();
    void draw_electrical_visualization();
    void draw_bonds_visualization();
    
    // Rendering helpers
    void render_particles_individual();
    void render_black_hole();
    void render_lit_sphere(const Vector3& position, float radius, Color color);
    
    // Lighting management
    void initialize_lighting_system();
    void cleanup_lighting_system();
    void setup_scene_lights();
    Color get_temperature_color(float temperature);
}; 