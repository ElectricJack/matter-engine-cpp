#include "particle_system.h"
#include <chrono>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <random>

// Static member definitions
std::vector<MaterialProperties> ParticleSystem::material_properties_;
std::unordered_map<std::pair<MaterialType, MaterialType>, float,
                   std::hash<std::pair<MaterialType, MaterialType>>> ParticleSystem::adhesion_matrix_;
std::vector<ChemicalReaction> ParticleSystem::chemical_reactions_;
bool ParticleSystem::static_data_initialized_ = false;



// Static material properties initialization
void ParticleSystem::initialize_material_properties() {
    material_properties_.resize(static_cast<size_t>(MaterialType::COUNT));
    
    // Initialize material properties according to the design document
    material_properties_[static_cast<size_t>(MaterialType::Water)] = 
        MaterialProperties("Water", 1000.0f, 4184.0f, 0.6f, 334000.0f, 2260000.0f, 0.95f, 
                          5e-6f, 80.0f, 3e6f, 0.0f, 100.0f, 0.05f, BLUE, PhaseState::Liquid);
    
    material_properties_[static_cast<size_t>(MaterialType::Oxygen)] = 
        MaterialProperties("Oxygen", 1.43f, 918.0f, 0.026f, 139000.0f, 213000.0f, 0.20f,
                          1e-18f, 1.0005f, 3e6f, -218.8f, -183.0f, 0.0f, SKYBLUE, PhaseState::Gas);
    
    material_properties_[static_cast<size_t>(MaterialType::Hydrogen)] = 
        MaterialProperties("Hydrogen", 0.09f, 14300.0f, 0.18f, 60000.0f, 455000.0f, 0.10f,
                          1e-18f, 1.0001f, 3e7f, -259.1f, -252.9f, 0.0f, LIGHTGRAY, PhaseState::Gas);
    
    material_properties_[static_cast<size_t>(MaterialType::Carbon)] = 
        MaterialProperties("Carbon", 1800.0f, 710.0f, 1.5f, 113000.0f, 360000.0f, 0.80f,
                          1e4f, 10.0f, 2e6f, 3550.0f, 4827.0f, 0.65f, BLACK, PhaseState::Solid);
    
    material_properties_[static_cast<size_t>(MaterialType::Rock)] = 
        MaterialProperties("Rock", 2700.0f, 800.0f, 2.5f, 250000.0f, 1000000.0f, 0.90f,
                          1e-8f, 5.0f, 3e6f, 1200.0f, 2800.0f, 0.85f, GRAY, PhaseState::Solid);
    
    material_properties_[static_cast<size_t>(MaterialType::Wood)] = 
        MaterialProperties("Wood", 600.0f, 1700.0f, 0.12f, 200000.0f, 0.0f, 0.90f,
                          1e-9f, 4.0f, 2e6f, 300.0f, 0.0f, 0.60f, BROWN, PhaseState::Solid);
    
    material_properties_[static_cast<size_t>(MaterialType::Plant)] = 
        MaterialProperties("Plant", 400.0f, 2500.0f, 0.20f, 150000.0f, 0.0f, 0.90f,
                          1e-8f, 8.0f, 2e6f, 200.0f, 0.0f, 0.50f, GREEN, PhaseState::Solid);
    
    material_properties_[static_cast<size_t>(MaterialType::Iron)] = 
        MaterialProperties("Iron", 7874.0f, 450.0f, 80.0f, 272000.0f, 6200000.0f, 0.30f,
                          1e7f, 1.0f, 1e8f, 1538.0f, 2862.0f, 0.80f, DARKGRAY, PhaseState::Solid);
    
    material_properties_[static_cast<size_t>(MaterialType::Copper)] = 
        MaterialProperties("Copper", 8960.0f, 385.0f, 400.0f, 205000.0f, 4700000.0f, 0.05f,
                          5.9e7f, 1.0f, 1e8f, 1085.0f, 2562.0f, 0.75f, Color{184, 115, 51, 255}, PhaseState::Solid);
    
    material_properties_[static_cast<size_t>(MaterialType::Gold)] = 
        MaterialProperties("Gold", 19320.0f, 129.0f, 320.0f, 63700.0f, 1630000.0f, 0.02f,
                          4.1e7f, 1.0f, 1e8f, 1064.0f, 2856.0f, 0.70f, GOLD, PhaseState::Solid);
    
    material_properties_[static_cast<size_t>(MaterialType::Oil)] = 
        MaterialProperties("Oil", 800.0f, 2000.0f, 0.15f, 200000.0f, 800000.0f, 0.95f,
                          1e-10f, 3.0f, 5e6f, -40.0f, 150.0f, 0.10f, Color{139, 69, 19, 255}, PhaseState::Liquid);
    
    material_properties_[static_cast<size_t>(MaterialType::Uranium)] = 
        MaterialProperties("Uranium", 19050.0f, 116.0f, 27.0f, 50000.0f, 600000.0f, 0.30f,
                          3e6f, 1.0f, 1e7f, 1135.0f, 4131.0f, 0.90f, Color{0, 128, 0, 255}, PhaseState::Solid);
    
    material_properties_[static_cast<size_t>(MaterialType::IronOxide)] = 
        MaterialProperties("Iron Oxide", 5242.0f, 650.0f, 1.0f, 300000.0f, 1500000.0f, 0.85f,
                          1e-6f, 12.0f, 1e6f, 1565.0f, 0.0f, 0.70f, Color{139, 69, 19, 255}, PhaseState::Solid);
    
    material_properties_[static_cast<size_t>(MaterialType::Plasma)] = 
        MaterialProperties("Plasma", 1.0f, 5000.0f, 10.0f, 0.0f, 0.0f, 1.0f,
                          1e6f, 1.0f, 1e10f, 10000.0f, 15000.0f, 0.0f, Color{255, 0, 255, 255}, PhaseState::Plasma);
}

void ParticleSystem::initialize_adhesion_matrix() {
    // Initialize adhesion matrix according to the design document
    adhesion_matrix_[{MaterialType::Rock, MaterialType::Rock}] = 0.85f;
    adhesion_matrix_[{MaterialType::Rock, MaterialType::Iron}] = 0.80f;
    adhesion_matrix_[{MaterialType::Rock, MaterialType::Copper}] = 0.75f;
    adhesion_matrix_[{MaterialType::Rock, MaterialType::Water}] = 0.10f;
    adhesion_matrix_[{MaterialType::Rock, MaterialType::Oil}] = 0.05f;
    adhesion_matrix_[{MaterialType::Rock, MaterialType::Wood}] = 0.20f;
    
    adhesion_matrix_[{MaterialType::Iron, MaterialType::Rock}] = 0.80f;
    adhesion_matrix_[{MaterialType::Iron, MaterialType::Iron}] = 0.80f;
    adhesion_matrix_[{MaterialType::Iron, MaterialType::Copper}] = 0.70f;
    adhesion_matrix_[{MaterialType::Iron, MaterialType::Water}] = 0.05f;
    adhesion_matrix_[{MaterialType::Iron, MaterialType::Oil}] = 0.05f;
    
    adhesion_matrix_[{MaterialType::Copper, MaterialType::Rock}] = 0.75f;
    adhesion_matrix_[{MaterialType::Copper, MaterialType::Iron}] = 0.70f;
    adhesion_matrix_[{MaterialType::Copper, MaterialType::Copper}] = 0.75f;
    adhesion_matrix_[{MaterialType::Copper, MaterialType::Oil}] = 0.05f;
    
    adhesion_matrix_[{MaterialType::Gold, MaterialType::Rock}] = 0.70f;
    adhesion_matrix_[{MaterialType::Gold, MaterialType::Iron}] = 0.65f;
    adhesion_matrix_[{MaterialType::Gold, MaterialType::Gold}] = 0.70f;
    
    adhesion_matrix_[{MaterialType::Wood, MaterialType::Rock}] = 0.20f;
    adhesion_matrix_[{MaterialType::Wood, MaterialType::Iron}] = 0.10f;
    adhesion_matrix_[{MaterialType::Wood, MaterialType::Water}] = 0.30f;
    adhesion_matrix_[{MaterialType::Wood, MaterialType::Oil}] = 0.15f;
    adhesion_matrix_[{MaterialType::Wood, MaterialType::Wood}] = 0.60f;
    adhesion_matrix_[{MaterialType::Wood, MaterialType::Plant}] = 0.50f;
    
    adhesion_matrix_[{MaterialType::Plant, MaterialType::Wood}] = 0.50f;
    adhesion_matrix_[{MaterialType::Plant, MaterialType::Water}] = 0.40f;
    adhesion_matrix_[{MaterialType::Plant, MaterialType::Plant}] = 0.50f;
    
    adhesion_matrix_[{MaterialType::Water, MaterialType::Water}] = 0.05f;
    adhesion_matrix_[{MaterialType::Water, MaterialType::Wood}] = 0.30f;
    adhesion_matrix_[{MaterialType::Water, MaterialType::Plant}] = 0.40f;
    adhesion_matrix_[{MaterialType::Water, MaterialType::Rock}] = 0.10f;
    
    adhesion_matrix_[{MaterialType::Oil, MaterialType::Water}] = 0.05f;
    adhesion_matrix_[{MaterialType::Oil, MaterialType::Wood}] = 0.15f;
    adhesion_matrix_[{MaterialType::Oil, MaterialType::Oil}] = 0.10f;
    adhesion_matrix_[{MaterialType::Oil, MaterialType::Rock}] = 0.05f;
    
    adhesion_matrix_[{MaterialType::Carbon, MaterialType::Carbon}] = 0.65f;
    adhesion_matrix_[{MaterialType::Carbon, MaterialType::Iron}] = 0.40f;
    
    adhesion_matrix_[{MaterialType::Uranium, MaterialType::Rock}] = 0.50f;
    adhesion_matrix_[{MaterialType::Uranium, MaterialType::Iron}] = 0.60f;
    adhesion_matrix_[{MaterialType::Uranium, MaterialType::Uranium}] = 0.90f;
    
    // Make adhesion matrix symmetric
    auto keys = adhesion_matrix_;
    for (const auto& pair : keys) {
        MaterialType mat1 = pair.first.first;
        MaterialType mat2 = pair.first.second;
        float value = pair.second;
        adhesion_matrix_[{mat2, mat1}] = value;
    }
}

void ParticleSystem::initialize_chemical_reactions() {
    // Wood + O2 → Carbon + Water
    ChemicalReaction wood_combustion(300.0f, -1.8e7f, 0.001f);
    wood_combustion.reactants[MaterialType::Wood] = 1;
    wood_combustion.reactants[MaterialType::Oxygen] = 2;
    wood_combustion.products[MaterialType::Carbon] = 1;
    wood_combustion.products[MaterialType::Water] = 2;
    chemical_reactions_.push_back(wood_combustion);
    
    // Hydrogen + O2 → Water
    ChemicalReaction hydrogen_combustion(600.0f, -2.86e8f, 0.01f);
    hydrogen_combustion.reactants[MaterialType::Hydrogen] = 2;
    hydrogen_combustion.reactants[MaterialType::Oxygen] = 1;
    hydrogen_combustion.products[MaterialType::Water] = 2;
    chemical_reactions_.push_back(hydrogen_combustion);
    
    // Iron + O2 → Iron Oxide (rust)
    ChemicalReaction iron_oxidation(50.0f, -8e4f, 0.0001f);
    iron_oxidation.reactants[MaterialType::Iron] = 1;
    iron_oxidation.reactants[MaterialType::Oxygen] = 1;
    iron_oxidation.products[MaterialType::IronOxide] = 1;
    chemical_reactions_.push_back(iron_oxidation);
}

ParticleSystem::ParticleSystem() 
    : spatial_hash_(nullptr), physics_time_ms_(0.0f),
      instanced_rendering_initialized_(false), use_instanced_rendering_(true) {
    
    // Initialize static data if not already done
    if (!static_data_initialized_) {
        initialize_material_properties();
        initialize_adhesion_matrix();
        initialize_chemical_reactions();
        static_data_initialized_ = true;
    }
}

ParticleSystem::~ParticleSystem() {
    cleanup();
}

void ParticleSystem::initialize() {
    printf("Initializing material-based particle system...\n"); 
    
    // Create spatial hash for efficient neighbor queries
    spatial_hash_ = sh_create(SPATIAL_CELL_SIZE, 1024); // Start with 1024 initial capacity
    if (!spatial_hash_) {
        printf("Error: Failed to create spatial hash\n");
        return;
    }
    
    // Reserve space for particles (start with reasonable capacity)
    const size_t initial_capacity = 10000;
    pos_x_.reserve(initial_capacity);
    pos_y_.reserve(initial_capacity);
    pos_z_.reserve(initial_capacity);
    vel_x_.reserve(initial_capacity);
    vel_y_.reserve(initial_capacity);
    vel_z_.reserve(initial_capacity);
    temperature_.reserve(initial_capacity);
    charge_.reserve(initial_capacity);
    voltage_.reserve(initial_capacity);
    type_id_.reserve(initial_capacity);
    phase_state_.reserve(initial_capacity);
    active_.reserve(initial_capacity);
    bonds_.reserve(initial_capacity);
    
    // Reserve space for particle references
    particle_refs_.reserve(initial_capacity);
    
    // Initialize black hole at center
    black_hole_.position = {0, 0, 0};
    black_hole_.mass = 100.0f;
    black_hole_.radius = 2.0f;
    black_hole_.color = BLACK;
    
    // Initialize instanced rendering
    initialize_instanced_rendering();
    
    printf("Material-based particle system initialized successfully!\n");
    printf("  Materials available: %zu\n", material_properties_.size());
    printf("  Chemical reactions: %zu\n", chemical_reactions_.size());
    printf("  Adhesion matrix entries: %zu\n", adhesion_matrix_.size());
    printf("  Black hole mass: %.2f\n", black_hole_.mass);
    printf("  Spatial cell size: %.2f\n", SPATIAL_CELL_SIZE);
    printf("  Instanced rendering: %s\n", instanced_rendering_initialized_ ? "ENABLED" : "DISABLED");
}

void ParticleSystem::cleanup() {
    printf("Cleaning up particle system...\n");
    
    // Cleanup lighting resources
    cleanup_lighting_system();
    
    // Cleanup instanced rendering resources
    cleanup_instanced_rendering();
    
    // Clear all particle data
    pos_x_.clear();
    pos_y_.clear();
    pos_z_.clear();
    vel_x_.clear();
    vel_y_.clear();
    vel_z_.clear();
    temperature_.clear();
    charge_.clear();
    voltage_.clear();
    type_id_.clear();
    phase_state_.clear();
    active_.clear();
    bonds_.clear();
    free_indices_.clear();
    
    particle_types_.clear();
    particle_refs_.clear();
    
    // Destroy spatial hash
    if (spatial_hash_) {
        sh_destroy(spatial_hash_);
        spatial_hash_ = nullptr;
    }
    
    printf("Particle system cleanup complete.\n");
}

void ParticleSystem::reset() {
    printf("Resetting particle system...\n");
    cleanup();
    initialize();
}

uint32_t ParticleSystem::create_particle_type(float radius, MaterialType material, float mass, Color color) {
    uint32_t type_id = static_cast<uint32_t>(particle_types_.size());
    particle_types_.emplace_back(radius, material, mass, color);
    printf("Created particle type %u: material=%s, radius=%.2f, mass=%.2f\n", 
           type_id, material_properties_[static_cast<size_t>(material)].name, radius, mass);
    return type_id;
}

void ParticleSystem::add_particle(uint32_t type_id, const Vector3& position, const Vector3& velocity, 
                                 float temperature, float charge) {
    if (type_id >= particle_types_.size()) {
        printf("Error: Invalid particle type ID %u\n", type_id);
        return;
    }
    
    uint32_t index;
    const ParticleType& type = particle_types_[type_id];
    const MaterialProperties& material = material_properties_[static_cast<size_t>(type.material)];
    
    // Use free index if available, otherwise add to end
    if (!free_indices_.empty()) {
        index = free_indices_.back();
        free_indices_.pop_back();
        
        // Reuse existing slot
        pos_x_[index] = position.x;
        pos_y_[index] = position.y;
        pos_z_[index] = position.z;
        vel_x_[index] = velocity.x;
        vel_y_[index] = velocity.y;
        vel_z_[index] = velocity.z;
        temperature_[index] = temperature;
        charge_[index] = charge;
        voltage_[index] = 0.0f;
        type_id_[index] = type_id;
        phase_state_[index] = material.default_phase;
        active_[index] = true;
        bonds_[index].clear();
    } else {
        // Add new particle at end
        index = static_cast<uint32_t>(pos_x_.size());
        pos_x_.push_back(position.x);
        pos_y_.push_back(position.y);
        pos_z_.push_back(position.z);
        vel_x_.push_back(velocity.x);
        vel_y_.push_back(velocity.y);
        vel_z_.push_back(velocity.z);
        temperature_.push_back(temperature);
        charge_.push_back(charge);
        voltage_.push_back(0.0f);
        type_id_.push_back(type_id);
        phase_state_.push_back(material.default_phase);
        active_.push_back(true);
        bonds_.emplace_back();
    }
    
    printf("Added particle %u (%s) at (%.2f, %.2f, %.2f) T=%.1f°C Q=%.2f\n",
           index, material.name, position.x, position.y, position.z, temperature, charge);
}

void ParticleSystem::remove_particle(uint32_t particle_index) {
    if (particle_index >= active_.size() || !active_[particle_index]) {
        return;
    }
    
    // Mark as inactive and add to free list
    active_[particle_index] = false;
    free_indices_.push_back(particle_index);
}

void ParticleSystem::update(float dt) {
    PROFILE_SECTION("Total Physics Update");
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Clear and repopulate spatial hash with current particle positions
    {
        PROFILE_SECTION("Populate Spatial Hash");
        populate_spatial_hash();
    }
    
    // Apply gravitational forces (legacy physics)
    if (gravity_simulation_) {
        PROFILE_SECTION("Apply Gravitational Forces");
        apply_gravitational_forces(dt);
    }
    
    // Material-based physics simulations
    if (thermal_simulation_) {
        PROFILE_SECTION("Thermal Simulation");
        update_thermal_simulation(dt);
    }
    
    if (electrical_simulation_) {
        PROFILE_SECTION("Electrical Simulation");
        update_electrical_simulation(dt);
    }
    
    if (chemical_simulation_) {
        PROFILE_SECTION("Chemical Reactions");
        update_chemical_reactions(dt);
    }
    
    if (bonding_simulation_) {
        PROFILE_SECTION("Bonding System");
        update_bonding_system(dt);
    }
    
    // Handle particle collisions and merging using spatial queries
    {
        PROFILE_SECTION("Handle Collisions");
        handle_particle_collisions_spatial();
    }
    
    // Integrate all particles
    {
        PROFILE_SECTION("Integrate Particles");
        integrate_particles(dt);
        check_bounds();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    physics_time_ms_ = std::chrono::duration<float, std::milli>(end_time - start_time).count();
}

void ParticleSystem::apply_gravitational_forces(float dt) {
    PROFILE_SECTION("Total Gravitational Forces");
    
    // Apply black hole forces to all particles (only if enabled)
    if (black_hole_enabled_) {
        PROFILE_SECTION("Black Hole Forces");
        apply_black_hole_forces(dt);
    }
    
    // Apply particle-particle forces using spatial optimization
    {
        PROFILE_SECTION("Particle-Particle Forces");
        apply_particle_particle_forces_spatial(dt);
    }
}

void ParticleSystem::apply_black_hole_forces(float dt) {
    PROFILE_SECTION("Black Hole Force Calculation");
    
    // Vectorized force application using Structure of Arrays
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        const ParticleType& type = particle_types_[type_id_[i]];
        
        // Calculate vector from particle to black hole
        float dx = black_hole_.position.x - pos_x_[i];
        float dy = black_hole_.position.y - pos_y_[i];
        float dz = black_hole_.position.z - pos_z_[i];
        
        float distance_sq = dx*dx + dy*dy + dz*dz;
        float distance = sqrtf(distance_sq);
        
        if (distance > MIN_DISTANCE) {
            // F = G * m1 * m2 / r^2
            float force_magnitude = GRAVITATIONAL_CONSTANT * type.mass * black_hole_.mass / distance_sq;
            
            // Normalize direction vector
            float inv_distance = 1.0f / distance;
            float force_x = dx * inv_distance * force_magnitude;
            float force_y = dy * inv_distance * force_magnitude;
            float force_z = dz * inv_distance * force_magnitude;
            
            // Apply force as acceleration (F = ma, so a = F/m)
            float inv_mass = 1.0f / type.mass;
            vel_x_[i] += force_x * inv_mass * dt;
            vel_y_[i] += force_y * inv_mass * dt;
            vel_z_[i] += force_z * inv_mass * dt;
        }
    }
}

void ParticleSystem::apply_particle_particle_forces_spatial(float dt) {
    PROFILE_SECTION("Spatial Force Calculation");
    
    // Buffer for neighbor queries
    void* neighbors[MAX_NEIGHBORS];
    
    // Process each active particle
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        const ParticleType& particle_type = particle_types_[type_id_[i]];
        
        // Calculate gravity radius for this particle type
        float gravity_radius = calculate_gravity_radius(particle_type.mass);
        
        float px = pos_x_[i];
        float py = pos_y_[i];
        float pz = pos_z_[i];
        
        // Query neighbors within gravity influence radius
        int neighbor_count;
        {
            PROFILE_SECTION("Spatial Hash Query");
            neighbor_count = sh_query_radius(spatial_hash_, px, py, pz, gravity_radius, neighbors, MAX_NEIGHBORS);
        }
        
        // Apply forces from each neighbor
        {
            PROFILE_SECTION("Force Application");
            for (int n = 0; n < neighbor_count; ++n) {
                ParticleRef* neighbor_ref = (ParticleRef*)neighbors[n];
                uint32_t neighbor_idx = neighbor_ref->particle_index;
                
                // Skip self-reference
                if (neighbor_idx == i) continue;
                
                // Skip inactive neighbors
                if (neighbor_idx >= active_.size() || !active_[neighbor_idx]) continue;
                
                const ParticleType& neighbor_type = particle_types_[type_id_[neighbor_idx]];
                
                float nx = pos_x_[neighbor_idx];
                float ny = pos_y_[neighbor_idx];
                float nz = pos_z_[neighbor_idx];
                
                // Calculate distance vector
                float dx = nx - px;
                float dy = ny - py;
                float dz = nz - pz;
                
                float distance_sq = dx*dx + dy*dy + dz*dz;
                float distance = sqrtf(distance_sq);
                
                // Apply weak gravitational force (much weaker than black hole)
                if (distance > MIN_DISTANCE) {
                    // Use very weak force to preserve orbital stability
                    float force_magnitude = GRAVITATIONAL_CONSTANT * particle_type.mass * neighbor_type.mass / distance_sq;
                    
                    float inv_distance = 1.0f / distance;
                    float force_x = dx * inv_distance * force_magnitude;
                    float force_y = dy * inv_distance * force_magnitude;
                    float force_z = dz * inv_distance * force_magnitude;
                    
                    // Apply force as acceleration (F = ma, so a = F/m)
                    float inv_mass = 1.0f / particle_type.mass;
                    vel_x_[i] += force_x * inv_mass * dt;
                    vel_y_[i] += force_y * inv_mass * dt;
                    vel_z_[i] += force_z * inv_mass * dt;
                }
            }
        }
    }
}

void ParticleSystem::handle_particle_collisions_spatial() {
    PROFILE_SECTION("Collision Detection");
    
    // Buffer for neighbor queries
    void* neighbors[MAX_NEIGHBORS];
    
    // Collect collision pairs to avoid modifying arrays while iterating
    struct CollisionPair {
        uint32_t particle1_idx;
        uint32_t particle2_idx;
        float distance;
    };
    
    std::vector<CollisionPair> collisions;
    
    // Process each active particle
    {
        PROFILE_SECTION("Find Collision Pairs");
        for (uint32_t i = 0; i < pos_x_.size(); ++i) {
            if (!active_[i]) continue;
            
            const ParticleType& particle_type = particle_types_[type_id_[i]];
            
            // Calculate collision radius for this particle type
            float collision_radius = calculate_collision_radius(particle_type.radius);
            
            float px = pos_x_[i];
            float py = pos_y_[i];
            float pz = pos_z_[i];
            
            // Query neighbors within collision radius
            int neighbor_count = sh_query_radius(spatial_hash_, px, py, pz, collision_radius, neighbors, MAX_NEIGHBORS);
            
            // Check for collisions with each neighbor
            for (int n = 0; n < neighbor_count; ++n) {
                ParticleRef* neighbor_ref = (ParticleRef*)neighbors[n];
                uint32_t neighbor_idx = neighbor_ref->particle_index;
                
                // Skip self-reference
                if (neighbor_idx == i) continue;
                
                // Only check each pair once (avoid duplicate collision pairs)
                if (neighbor_idx < i) continue;
                
                // Skip inactive neighbors
                if (neighbor_idx >= active_.size() || !active_[neighbor_idx]) continue;
                
                float nx = pos_x_[neighbor_idx];
                float ny = pos_y_[neighbor_idx];
                float nz = pos_z_[neighbor_idx];
                
                // Calculate distance
                float dx = nx - px;
                float dy = ny - py;
                float dz = nz - pz;
                float distance = sqrtf(dx*dx + dy*dy + dz*dz);
                
                // Check if collision occurs
                if (distance < COLLISION_DISTANCE) {
                    collisions.push_back({i, neighbor_idx, distance});
                }
            }
        }
    }
    
    // Process collisions
    for (const auto& collision : collisions) {
        uint32_t p1 = collision.particle1_idx;
        uint32_t p2 = collision.particle2_idx;
        
        // Skip if either particle is already inactive
        if (!active_[p1] || !active_[p2]) continue;
        
        // Get particle properties
        const ParticleType& type1 = particle_types_[type_id_[p1]];
        const ParticleType& type2 = particle_types_[type_id_[p2]];
        
        // Calculate combined mass and momentum conservation
        float mass1 = type1.mass;
        float mass2 = type2.mass;
        float total_mass = mass1 + mass2;
        
        // Weighted average position
        float new_x = (pos_x_[p1] * mass1 + pos_x_[p2] * mass2) / total_mass;
        float new_y = (pos_y_[p1] * mass1 + pos_y_[p2] * mass2) / total_mass;
        float new_z = (pos_z_[p1] * mass1 + pos_z_[p2] * mass2) / total_mass;
        
        // Momentum conservation
        float new_vel_x = (vel_x_[p1] * mass1 + vel_x_[p2] * mass2) / total_mass;
        float new_vel_y = (vel_y_[p1] * mass1 + vel_y_[p2] * mass2) / total_mass;
        float new_vel_z = (vel_z_[p1] * mass1 + vel_z_[p2] * mass2) / total_mass;
        
        // Add some angular velocity for visual interest
        float angular_boost = 0.1f * (mass1 + mass2);
        new_vel_x += ((float)rand() / RAND_MAX - 0.5f) * angular_boost;
        new_vel_y += ((float)rand() / RAND_MAX - 0.5f) * angular_boost;
        new_vel_z += ((float)rand() / RAND_MAX - 0.5f) * angular_boost;
        
        // Average temperature
        float new_temp = (temperature_[p1] + temperature_[p2]) * 0.5f + 10.0f; // Heat from collision
        
        // Create new particle type for the merged particle (use the heavier one's type as base)
        uint32_t new_type_id = (mass1 >= mass2) ? type_id_[p1] : type_id_[p2];
        
        // Remove both particles
        remove_particle(p1);
        remove_particle(p2);
        
        // Add the merged particle
        add_particle(new_type_id, Vector3{new_x, new_y, new_z}, 
                    Vector3{new_vel_x, new_vel_y, new_vel_z}, new_temp, 0.0f);
        
        // Only process one collision per frame to avoid complex index management
        break;
    }
}

void ParticleSystem::populate_spatial_hash() {
    PROFILE_SECTION("Spatial Hash Population");
    
    // Clear the spatial hash
    {
        PROFILE_SECTION("Clear Spatial Hash");
        sh_clear(spatial_hash_);
    }
    
    // Clear particle references and prepare for new ones
    {
        PROFILE_SECTION("Clear Particle Refs");
        particle_refs_.clear();
    }
    
    // Insert all active particles into spatial hash
    {
        PROFILE_SECTION("Insert Particles");
        for (uint32_t i = 0; i < pos_x_.size(); ++i) {
            if (!active_[i]) continue;
            
            // Create particle reference
            particle_refs_.emplace_back(i);
            ParticleRef* ref = &particle_refs_.back();
            
            // Insert into spatial hash at particle position
            sh_insert(spatial_hash_, pos_x_[i], pos_y_[i], pos_z_[i], ref);
        }
    }
}

float ParticleSystem::calculate_gravity_radius(float mass) const {
    return GRAVITY_BASE_RADIUS + (mass * MASS_RADIUS_MULTIPLIER);
}

float ParticleSystem::calculate_collision_radius(float radius) const {
    return radius * 2.0f + COLLISION_DISTANCE; // Small buffer for collision detection
}

void ParticleSystem::integrate_particles(float dt) {
    // Euler integration with damping
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        // Update positions
        pos_x_[i] += vel_x_[i] * dt;
        pos_y_[i] += vel_y_[i] * dt;
        pos_z_[i] += vel_z_[i] * dt;
        
        // Apply damping
        vel_x_[i] *= DAMPING;
        vel_y_[i] *= DAMPING;
        vel_z_[i] *= DAMPING;
        
        // Update temperature based on velocity (kinetic energy)
        float speed_sq = vel_x_[i]*vel_x_[i] + vel_y_[i]*vel_y_[i] + vel_z_[i]*vel_z_[i];
        temperature_[i] = 20.0f + speed_sq * 0.1f; // Base temp + kinetic heating
    }
}

void ParticleSystem::check_bounds() {
    // Reset particles that go too far
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        float distance_from_center = sqrtf(pos_x_[i]*pos_x_[i] + 
                                          pos_y_[i]*pos_y_[i] + 
                                          pos_z_[i]*pos_z_[i]);
        
        if (distance_from_center > MAX_DISTANCE) {
            // Reset to near center with small random velocity
            pos_x_[i] = ((float)rand() / RAND_MAX - 0.5f) * 4.0f;
            pos_y_[i] = ((float)rand() / RAND_MAX - 0.5f) * 4.0f;
            pos_z_[i] = ((float)rand() / RAND_MAX - 0.5f) * 4.0f;
            vel_x_[i] = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
            vel_y_[i] = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
            vel_z_[i] = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
            temperature_[i] = 20.0f;
        }
    }
}

// Material physics simulation implementations
void ParticleSystem::update_thermal_simulation(float dt) {
    PROFILE_SECTION("Complete Thermal Simulation");
    
    // Apply thermal conduction between neighboring particles
    {
        PROFILE_SECTION("Thermal Conduction");
        apply_thermal_conduction(dt);
    }
    
    // Apply phase changes based on temperature
    {
        PROFILE_SECTION("Phase Changes");
        apply_phase_changes(dt);
    }
    
    // Apply radiative cooling
    {
        PROFILE_SECTION("Radiative Cooling");
        apply_radiative_cooling(dt);
    }
}

void ParticleSystem::update_electrical_simulation(float dt) {
    PROFILE_SECTION("Complete Electrical Simulation");
    
    // Apply electrical conduction between particles
    {
        PROFILE_SECTION("Electrical Conduction");
        apply_electrical_conduction(dt);
    }
    
    // Apply Joule heating from electrical currents
    {
        PROFILE_SECTION("Joule Heating");
        apply_joule_heating(dt);
    }
    
    // Check for dielectric breakdown
    {
        PROFILE_SECTION("Dielectric Breakdown");
        check_dielectric_breakdown();
    }
}

void ParticleSystem::update_chemical_reactions(float dt) {
    PROFILE_SECTION("Complete Chemical Reactions");
    process_chemical_reactions(dt);
}

void ParticleSystem::update_bonding_system(float dt) {
    PROFILE_SECTION("Complete Bonding System");
    
    // Update particle bonds (formation and breaking)
    {
        PROFILE_SECTION("Bond Updates");
        update_particle_bonds(dt);
    }
    
    // Apply forces from bonds
    {
        PROFILE_SECTION("Bond Forces");
        apply_bond_forces(dt);
    }
}

void ParticleSystem::apply_thermal_conduction(float dt) {
    void* neighbors[MAX_NEIGHBORS];
    
    // Process thermal conduction between neighboring particles
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        const ParticleType& particle_type = particle_types_[type_id_[i]];
        const MaterialProperties& material = material_properties_[static_cast<size_t>(particle_type.material)];
        
        float px = pos_x_[i];
        float py = pos_y_[i];
        float pz = pos_z_[i];
        
        // Query neighbors within thermal conduction range
        float thermal_radius = particle_type.radius * 3.0f;
        int neighbor_count = sh_query_radius(spatial_hash_, px, py, pz, thermal_radius, neighbors, MAX_NEIGHBORS);
        
        for (int n = 0; n < neighbor_count; ++n) {
            ParticleRef* neighbor_ref = (ParticleRef*)neighbors[n];
            uint32_t neighbor_idx = neighbor_ref->particle_index;
            
            if (neighbor_idx == i || neighbor_idx >= active_.size() || !active_[neighbor_idx]) continue;
            
            float distance = sqrtf(powf(pos_x_[neighbor_idx] - px, 2) + 
                                 powf(pos_y_[neighbor_idx] - py, 2) + 
                                 powf(pos_z_[neighbor_idx] - pz, 2));
            
            if (distance < thermal_radius) {
                float thermal_conductivity = calculate_thermal_conductivity_between(i, neighbor_idx);
                float temp_diff = temperature_[neighbor_idx] - temperature_[i];
                
                // Heat transfer: Q = k * A * (T2 - T1) / d * dt
                float area = 3.14159f * powf(std::min(particle_type.radius, 
                                                    particle_types_[type_id_[neighbor_idx]].radius), 2);
                float heat_transfer = thermal_conductivity * area * temp_diff / distance * dt * THERMAL_DIFFUSION_RATE;
                
                // Apply heat transfer
                float heat_capacity_i = material.heat_capacity * particle_type.mass;
                temperature_[i] += heat_transfer / heat_capacity_i;
            }
        }
    }
}

void ParticleSystem::apply_phase_changes(float dt) {
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        const ParticleType& particle_type = particle_types_[type_id_[i]];
        const MaterialProperties& material = material_properties_[static_cast<size_t>(particle_type.material)];
        
        float temp = temperature_[i];
        PhaseState current_phase = phase_state_[i];
        
        // Check for phase transitions
        if (current_phase == PhaseState::Solid && temp >= material.melt_point) {
            // Melting
            float energy_needed = material.melt_energy * particle_type.mass;
            if (temp > material.melt_point) {
                phase_state_[i] = PhaseState::Liquid;
                temperature_[i] -= energy_needed / (material.heat_capacity * particle_type.mass);
            }
        } else if (current_phase == PhaseState::Liquid && temp >= material.boil_point) {
            // Vaporization
            float energy_needed = material.vapor_energy * particle_type.mass;
            if (temp > material.boil_point) {
                phase_state_[i] = PhaseState::Gas;
                temperature_[i] -= energy_needed / (material.heat_capacity * particle_type.mass);
            }
        } else if (current_phase == PhaseState::Liquid && temp < material.melt_point) {
            // Freezing
            phase_state_[i] = PhaseState::Solid;
            temperature_[i] += material.melt_energy / (material.heat_capacity * particle_type.mass);
        } else if (current_phase == PhaseState::Gas && temp < material.boil_point) {
            // Condensation
            phase_state_[i] = PhaseState::Liquid;
            temperature_[i] += material.vapor_energy / (material.heat_capacity * particle_type.mass);
        }
    }
}

void ParticleSystem::apply_radiative_cooling(float dt) {
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        const ParticleType& particle_type = particle_types_[type_id_[i]];
        const MaterialProperties& material = material_properties_[static_cast<size_t>(particle_type.material)];
        
        // Stefan-Boltzmann cooling: P = σ * ε * A * T^4
        float temp_kelvin = temperature_[i] + 273.15f;
        float surface_area = 4.0f * 3.14159f * powf(particle_type.radius, 2);
        float power_radiated = STEFAN_BOLTZMANN * material.emissivity * surface_area * powf(temp_kelvin, 4);
        
        // Convert power to temperature change
        float heat_capacity = material.heat_capacity * particle_type.mass;
        float temp_change = power_radiated * dt / heat_capacity;
        
        temperature_[i] = std::max(temperature_[i] - temp_change, -273.15f); // Don't go below absolute zero
    }
}

void ParticleSystem::apply_electrical_conduction(float dt) {
    void* neighbors[MAX_NEIGHBORS];
    
    // Process electrical conduction between neighboring particles
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        const ParticleType& particle_type = particle_types_[type_id_[i]];
        
        float px = pos_x_[i];
        float py = pos_y_[i];
        float pz = pos_z_[i];
        
        // Query neighbors within electrical conduction range
        float electrical_radius = particle_type.radius * 2.0f;
        int neighbor_count = sh_query_radius(spatial_hash_, px, py, pz, electrical_radius, neighbors, MAX_NEIGHBORS);
        
        for (int n = 0; n < neighbor_count; ++n) {
            ParticleRef* neighbor_ref = (ParticleRef*)neighbors[n];
            uint32_t neighbor_idx = neighbor_ref->particle_index;
            
            if (neighbor_idx == i || neighbor_idx >= active_.size() || !active_[neighbor_idx]) continue;
            
            float distance = sqrtf(powf(pos_x_[neighbor_idx] - px, 2) + 
                                 powf(pos_y_[neighbor_idx] - py, 2) + 
                                 powf(pos_z_[neighbor_idx] - pz, 2));
            
            if (distance < electrical_radius) {
                float electrical_conductivity = calculate_electrical_conductivity_between(i, neighbor_idx);
                float voltage_diff = voltage_[neighbor_idx] - voltage_[i];
                
                // Current flow: I = σ * A * (V2 - V1) / d
                float area = 3.14159f * powf(std::min(particle_type.radius, 
                                                    particle_types_[type_id_[neighbor_idx]].radius), 2);
                float current = electrical_conductivity * area * voltage_diff / distance * dt;
                
                // Apply charge transfer
                charge_[i] += current;
                charge_[neighbor_idx] -= current;
                
                // Update voltage based on charge (simplified)
                voltage_[i] = charge_[i] * ELECTRICAL_RESISTANCE;
            }
        }
    }
}

void ParticleSystem::apply_joule_heating(float dt) {
    void* neighbors[MAX_NEIGHBORS];
    
    // Apply Joule heating from electrical currents
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        const ParticleType& particle_type = particle_types_[type_id_[i]];
        const MaterialProperties& material = material_properties_[static_cast<size_t>(particle_type.material)];
        
        // Calculate heating from current flow
        float current_density = charge_[i] / particle_type.mass;
        float resistance = ELECTRICAL_RESISTANCE / material.electrical_conductivity;
        float heat_generated = current_density * current_density * resistance * dt;
        
        // Apply heat to particle
        float heat_capacity = material.heat_capacity * particle_type.mass;
        temperature_[i] += heat_generated / heat_capacity;
    }
}

void ParticleSystem::check_dielectric_breakdown() {
    void* neighbors[MAX_NEIGHBORS];
    
    // Check for dielectric breakdown in gases
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        const ParticleType& particle_type = particle_types_[type_id_[i]];
        const MaterialProperties& material = material_properties_[static_cast<size_t>(particle_type.material)];
        
        // Only check gas particles for breakdown
        if (phase_state_[i] != PhaseState::Gas) continue;
        
        float px = pos_x_[i];
        float py = pos_y_[i];
        float pz = pos_z_[i];
        
        // Query neighbors to check electric field
        float breakdown_radius = particle_type.radius * 2.0f;
        int neighbor_count = sh_query_radius(spatial_hash_, px, py, pz, breakdown_radius, neighbors, MAX_NEIGHBORS);
        
        for (int n = 0; n < neighbor_count; ++n) {
            ParticleRef* neighbor_ref = (ParticleRef*)neighbors[n];
            uint32_t neighbor_idx = neighbor_ref->particle_index;
            
            if (neighbor_idx == i || neighbor_idx >= active_.size() || !active_[neighbor_idx]) continue;
            
            float distance = sqrtf(powf(pos_x_[neighbor_idx] - px, 2) + 
                                 powf(pos_y_[neighbor_idx] - py, 2) + 
                                 powf(pos_z_[neighbor_idx] - pz, 2));
            
            if (distance > 0.0f) {
                float electric_field = std::abs(voltage_[neighbor_idx] - voltage_[i]) / distance;
                
                if (electric_field > material.spark_threshold) {
                    // Dielectric breakdown - convert to plasma
                    phase_state_[i] = PhaseState::Plasma;
                    temperature_[i] = 10000.0f; // Very high temperature for plasma
                    break;
                }
            }
        }
    }
}

void ParticleSystem::process_chemical_reactions(float dt) {
    void* neighbors[MAX_NEIGHBORS];
    std::vector<uint32_t> nearby_particles;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    // Process each particle as a potential reaction site
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        const ParticleType& particle_type = particle_types_[type_id_[i]];
        float temp = temperature_[i];
        
        float px = pos_x_[i];
        float py = pos_y_[i];
        float pz = pos_z_[i];
        
        // Query neighbors for potential reactions
        float reaction_radius = particle_type.radius * 2.0f;
        int neighbor_count = sh_query_radius(spatial_hash_, px, py, pz, reaction_radius, neighbors, MAX_NEIGHBORS);
        
        nearby_particles.clear();
        nearby_particles.push_back(i);
        
        for (int n = 0; n < neighbor_count; ++n) {
            ParticleRef* neighbor_ref = (ParticleRef*)neighbors[n];
            uint32_t neighbor_idx = neighbor_ref->particle_index;
            
            if (neighbor_idx != i && neighbor_idx < active_.size() && active_[neighbor_idx]) {
                nearby_particles.push_back(neighbor_idx);
            }
        }
        
        // Check all chemical reactions
        for (const auto& reaction : chemical_reactions_) {
            if (temp >= reaction.activation_temperature && 
                can_react(reaction.reactants, nearby_particles) &&
                dis(gen) < reaction.probability) {
                
                // Perform the reaction
                Vector3 reaction_center = {px, py, pz};
                consume_reactants(reaction.reactants, nearby_particles);
                spawn_products(reaction.products, reaction_center, reaction.energy_change);
                
                // Only one reaction per particle per frame
                break;
            }
        }
    }
}

void ParticleSystem::update_particle_bonds(float dt) {
    void* neighbors[MAX_NEIGHBORS];
    
    // Update bonds for each particle
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        const ParticleType& particle_type = particle_types_[type_id_[i]];
        const MaterialProperties& material = material_properties_[static_cast<size_t>(particle_type.material)];
        
        float px = pos_x_[i];
        float py = pos_y_[i];
        float pz = pos_z_[i];
        
        // Query neighbors for potential bonding
        float bond_radius = BOND_FORMATION_DISTANCE;
        int neighbor_count = sh_query_radius(spatial_hash_, px, py, pz, bond_radius, neighbors, MAX_NEIGHBORS);
        
        for (int n = 0; n < neighbor_count; ++n) {
            ParticleRef* neighbor_ref = (ParticleRef*)neighbors[n];
            uint32_t neighbor_idx = neighbor_ref->particle_index;
            
            if (neighbor_idx == i || neighbor_idx >= active_.size() || !active_[neighbor_idx]) continue;
            
            float distance = sqrtf(powf(pos_x_[neighbor_idx] - px, 2) + 
                                 powf(pos_y_[neighbor_idx] - py, 2) + 
                                 powf(pos_z_[neighbor_idx] - pz, 2));
            
            if (distance < BOND_FORMATION_DISTANCE) {
                const ParticleType& neighbor_type = particle_types_[type_id_[neighbor_idx]];
                
                // Check if bond already exists
                bool bond_exists = false;
                for (const auto& bond : bonds_[i]) {
                    if (bond.particle_index == neighbor_idx) {
                        bond_exists = true;
                        break;
                    }
                }
                
                if (!bond_exists) {
                    // Check adhesion matrix for bonding probability
                    auto adhesion_key = std::make_pair(particle_type.material, neighbor_type.material);
                    auto it = adhesion_matrix_.find(adhesion_key);
                    if (it != adhesion_matrix_.end() && it->second > 0.1f) {
                        // Form bond
                        float bond_strength = it->second * 10.0f; // Scale for simulation
                        bonds_[i].emplace_back(neighbor_idx, bond_strength, distance);
                        bonds_[neighbor_idx].emplace_back(i, bond_strength, distance);
                    }
                }
            }
        }
        
        // Check for bond breaking
        auto& particle_bonds = bonds_[i];
        for (auto it = particle_bonds.begin(); it != particle_bonds.end();) {
            uint32_t bonded_idx = it->particle_index;
            
            if (bonded_idx >= active_.size() || !active_[bonded_idx]) {
                it = particle_bonds.erase(it);
                continue;
            }
            
            float distance = sqrtf(powf(pos_x_[bonded_idx] - px, 2) + 
                                 powf(pos_y_[bonded_idx] - py, 2) + 
                                 powf(pos_z_[bonded_idx] - pz, 2));
            
            float stretch = distance - it->rest_length;
            if (stretch > 0 && stretch * it->strength > BOND_BREAK_FORCE) {
                // Break bond
                it = particle_bonds.erase(it);
                
                // Remove corresponding bond from neighbor
                auto& neighbor_bonds = bonds_[bonded_idx];
                for (auto nit = neighbor_bonds.begin(); nit != neighbor_bonds.end(); ++nit) {
                    if (nit->particle_index == i) {
                        neighbor_bonds.erase(nit);
                        break;
                    }
                }
            } else {
                ++it;
            }
        }
    }
}

void ParticleSystem::apply_bond_forces(float dt) {
    // Apply forces from bonds
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        const ParticleType& particle_type = particle_types_[type_id_[i]];
        
        float px = pos_x_[i];
        float py = pos_y_[i];
        float pz = pos_z_[i];
        
        for (const auto& bond : bonds_[i]) {
            uint32_t bonded_idx = bond.particle_index;
            
            if (bonded_idx >= active_.size() || !active_[bonded_idx]) continue;
            
            float dx = pos_x_[bonded_idx] - px;
            float dy = pos_y_[bonded_idx] - py;
            float dz = pos_z_[bonded_idx] - pz;
            
            float distance = sqrtf(dx*dx + dy*dy + dz*dz);
            if (distance > 0.0f) {
                float stretch = distance - bond.rest_length;
                float force_magnitude = bond.strength * stretch;
                
                // Apply spring force
                float force_x = (dx / distance) * force_magnitude;
                float force_y = (dy / distance) * force_magnitude;
                float force_z = (dz / distance) * force_magnitude;
                
                // Apply force as acceleration
                float inv_mass = 1.0f / particle_type.mass;
                vel_x_[i] += force_x * inv_mass * dt;
                vel_y_[i] += force_y * inv_mass * dt;
                vel_z_[i] += force_z * inv_mass * dt;
            }
        }
    }
}

void ParticleSystem::render() {
    PROFILE_SECTION("Particle Rendering");
    
    // Render the black hole first (only if enabled)
    if (black_hole_enabled_) {
        render_black_hole();
    }
    
    // Choose rendering method based on mode and availability
    if (use_instanced_rendering_ && instanced_rendering_initialized_) {
        PROFILE_SECTION("Instanced Particle Rendering");
        render_particles_instanced();
    } else {
        // Fallback to individual rendering
        PROFILE_SECTION("Individual Particle Rendering");
        render_particles_individual();
    }
    
    // Render debug spatial information if enabled
    {
        PROFILE_SECTION("Debug Visualization");
        render_debug_spatial_info();
    }
}

void ParticleSystem::render_particles_individual() {
    // Initialize lighting system if not done yet (only try once to avoid repeated failures)
    static bool lighting_init_attempted = false;
    if (!lighting_initialized_ && !lighting_init_attempted) {
        initialize_lighting_system();
        lighting_init_attempted = true;
    }
    
    // Set up lighting system only if it's working
    bool use_lighting = lighting_initialized_ && sphere_model_initialized_ && lighting_shader_.id != 0;
    
    if (use_lighting) {
        // Update camera position for lighting calculations
        float cameraPos[3] = { camera_position_.x, camera_position_.y, camera_position_.z };
        SetShaderValue(lighting_shader_, lighting_shader_.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);
        
        // Update light values (ensure they're current)
        for (int i = 0; i < MAX_LIGHTS; i++) {
            if (lights_[i].enabled) {
                UpdateLightValues(lighting_shader_, lights_[i]);
            }
        }
    }
    
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        const ParticleType& type = particle_types_[type_id_[i]];
        Vector3 pos = {pos_x_[i], pos_y_[i], pos_z_[i]};
        
        // Visual radius based on phase state and temperature
        float visual_radius = type.radius;
        
        // Adjust radius based on phase
        switch (phase_state_[i]) {
            case PhaseState::Gas:
                visual_radius *= 1.5f; // Gases are more diffuse
                break;
            case PhaseState::Plasma:
                visual_radius *= 2.0f; // Plasma is very diffuse
                break;
            case PhaseState::Liquid:
                visual_radius *= 1.1f; // Liquids slightly larger
                break;
            case PhaseState::Solid:
            default:
                // Keep original radius
                break;
        }
        
        // Amplify for better visibility
        visual_radius *= (2.0f + type.mass * 0.1f);
        
        // Color based on material properties, temperature, and phase
        Color color = get_material_color(i);
        
        if (use_lighting) {
            // Use lit sphere model with proper scaling and positioning
            lighting_sphere_model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = color;
            DrawModelEx(lighting_sphere_model_, pos, Vector3{0, 1, 0}, 0.0f, Vector3{visual_radius, visual_radius, visual_radius}, color);
        } else {
            // Fallback to basic sphere
            DrawSphere(pos, visual_radius, color);
        }
    }
}

void ParticleSystem::render_black_hole() {
    // Draw the black hole with glowing effect
    DrawSphere(black_hole_.position, black_hole_.radius, BLACK);
    
    // Draw multiple wireframe spheres for glowing effect
    DrawSphereWires(black_hole_.position, black_hole_.radius, 16, 16, DARKGRAY);
    DrawSphereWires(black_hole_.position, black_hole_.radius * 1.2f, 12, 12, Color{64, 64, 64, 128});
    DrawSphereWires(black_hole_.position, black_hole_.radius * 1.5f, 8, 8, Color{32, 32, 32, 64});
    
    // Draw event horizon effect
    DrawSphereWires(black_hole_.position, black_hole_.radius * 2.0f, 6, 6, Color{128, 0, 128, 32});
}

void ParticleSystem::initialize_lighting_system() {
    if (lighting_initialized_) return;
    
    TraceLog(LOG_INFO, "Attempting to load lighting system...");
    
    // Try to load the lighting shader
    lighting_shader_ = LoadShader("shaders/lighting.vs", "shaders/lighting.fs");
    
    if (lighting_shader_.id != 0) {
        // Set up standard raylib shader locations
        lighting_shader_.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lighting_shader_, "viewPos");
        // NOTE: matModel and matNormal are automatically assigned by raylib
        
        // Set ambient light level
        int ambientLoc = GetShaderLocation(lighting_shader_, "ambient");
        float ambient[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        SetShaderValue(lighting_shader_, ambientLoc, ambient, SHADER_UNIFORM_VEC4);
        
        // Create sphere model for particles (proper lighting support)
        lighting_sphere_mesh_ = GenMeshSphere(1.0f, 16, 16); // Unit sphere, will scale in rendering
        lighting_sphere_model_ = LoadModelFromMesh(lighting_sphere_mesh_);
        lighting_sphere_model_.materials[0].shader = lighting_shader_;
        sphere_model_initialized_ = true;
        
        lighting_initialized_ = true;
        TraceLog(LOG_INFO, "Lighting shader loaded successfully (ID: %d)", lighting_shader_.id);
        TraceLog(LOG_INFO, "Sphere model created for particle rendering");
        
        // Set up scene lights
        setup_scene_lights();
    } else {
        TraceLog(LOG_WARNING, "Failed to load lighting shader (invalid ID), using default rendering");
        lighting_initialized_ = false;
    }
}

void ParticleSystem::cleanup_lighting_system() {
    if (sphere_model_initialized_) {
        UnloadModel(lighting_sphere_model_);
        sphere_model_initialized_ = false;
    }
    if (lighting_initialized_) {
        UnloadShader(lighting_shader_);
        lighting_initialized_ = false;
    }
}

void ParticleSystem::setup_scene_lights() {
    if (!lighting_initialized_) return;
    
    TraceLog(LOG_INFO, "Setting up scene lights...");
    
    // Initialize all lights as disabled
    for (int i = 0; i < MAX_LIGHTS; i++) {
        lights_[i] = (Light){ 0 };
    }
    
    // Create basic lighting setup
    lights_[0] = CreateLight(LIGHT_POINT, (Vector3){ -10.0f, 10.0f, -10.0f }, Vector3Zero(), WHITE, lighting_shader_);
    lights_[1] = CreateLight(LIGHT_POINT, (Vector3){ 10.0f, 10.0f, 10.0f }, Vector3Zero(), Color{255, 255, 200, 255}, lighting_shader_);
    lights_[2] = CreateLight(LIGHT_POINT, (Vector3){ 0.0f, 15.0f, 0.0f }, Vector3Zero(), Color{200, 200, 255, 255}, lighting_shader_);
    
    TraceLog(LOG_INFO, "Scene lights setup complete");
}

void ParticleSystem::render_lit_sphere(const Vector3& position, float radius, Color color) {
    // This function is now deprecated - using shader-based rendering instead
    DrawSphere(position, radius, color);
}

void ParticleSystem::set_camera_position(const Vector3& camera_pos) {
    camera_position_ = camera_pos;
}



Color ParticleSystem::get_temperature_color(float temperature) {
    // Color mapping: cold (blue) -> warm (red) -> hot (white)
    float t = (temperature - 20.0f) / 100.0f; // Normalize temperature
    t = std::max(0.0f, std::min(1.0f, t));
    
    if (t < 0.5f) {
        // Blue to red
        float blend = t * 2.0f;
        return Color{
            static_cast<unsigned char>(blend * 255),
            0,
            static_cast<unsigned char>((1.0f - blend) * 255),
            255
        };
    } else {
        // Red to white
        float blend = (t - 0.5f) * 2.0f;
        return Color{
            255,
            static_cast<unsigned char>(blend * 255),
            static_cast<unsigned char>(blend * 255),
            255
        };
    }
}

int ParticleSystem::get_particle_count() const {
    int total = 0;
    for (bool is_active : active_) {
        if (is_active) total++;
    }
    return total;
}

float ParticleSystem::get_physics_time_ms() const {
    return physics_time_ms_;
}

// Material properties access methods
const MaterialProperties& ParticleSystem::get_material_properties(MaterialType material) const {
    return material_properties_[static_cast<size_t>(material)];
}

const std::unordered_map<std::pair<MaterialType, MaterialType>, float,
                        std::hash<std::pair<MaterialType, MaterialType>>>& ParticleSystem::get_adhesion_matrix() {
    return adhesion_matrix_;
}

float ParticleSystem::get_average_temperature() const {
    if (temperature_.empty()) return 0.0f;
    
    float total_temp = 0.0f;
    int active_count = 0;
    
    for (uint32_t i = 0; i < temperature_.size(); ++i) {
        if (active_[i]) {
            total_temp += temperature_[i];
            active_count++;
        }
    }
    
    return active_count > 0 ? total_temp / active_count : 0.0f;
}

float ParticleSystem::get_total_electrical_energy() const {
    float total_energy = 0.0f;
    
    for (uint32_t i = 0; i < charge_.size(); ++i) {
        if (active_[i]) {
            total_energy += 0.5f * charge_[i] * voltage_[i]; // E = 0.5 * Q * V
        }
    }
    
    return total_energy;
}

int ParticleSystem::get_active_reactions_count() const {
    // Simple estimate - could be more sophisticated
    return static_cast<int>(chemical_reactions_.size());
}

int ParticleSystem::get_total_bonds_count() const {
    int total_bonds = 0;
    
    for (uint32_t i = 0; i < bonds_.size(); ++i) {
        if (active_[i]) {
            total_bonds += static_cast<int>(bonds_[i].size());
        }
    }
    
    return total_bonds / 2; // Each bond is counted twice
}

// Helper methods for material physics
Color ParticleSystem::get_material_color(uint32_t particle_index) const {
    if (particle_index >= type_id_.size() || !active_[particle_index]) {
        return WHITE;
    }
    
    const ParticleType& type = particle_types_[type_id_[particle_index]];
    const MaterialProperties& material = material_properties_[static_cast<size_t>(type.material)];
    
    // Base color from material properties
    Color base_color = material.base_color;
    
    // Modify color based on temperature and phase
    float temp = temperature_[particle_index];
    PhaseState phase = phase_state_[particle_index];
    
    // Temperature-based color modification
    if (temp > 500.0f) {
        // Hot materials glow
        float glow_factor = std::min(1.0f, (temp - 500.0f) / 1000.0f);
        base_color.r = static_cast<unsigned char>(std::min(255.0f, base_color.r + glow_factor * 100));
        base_color.g = static_cast<unsigned char>(std::min(255.0f, base_color.g + glow_factor * 50));
    }
    
    // Phase-based color modification
    switch (phase) {
        case PhaseState::Gas:
            base_color.a = 128; // Semi-transparent for gases
            break;
        case PhaseState::Plasma:
            return Color{255, 0, 255, 200}; // Bright magenta for plasma
        case PhaseState::Liquid:
            // Slightly more transparent than solids
            base_color.a = static_cast<unsigned char>(base_color.a * 0.9f);
            break;
        case PhaseState::Solid:
        default:
            // Keep original alpha
            break;
    }
    
    return base_color;
}

float ParticleSystem::calculate_thermal_conductivity_between(uint32_t p1, uint32_t p2) const {
    if (p1 >= type_id_.size() || p2 >= type_id_.size() || !active_[p1] || !active_[p2]) {
        return 0.0f;
    }
    
    const MaterialProperties& mat1 = material_properties_[static_cast<size_t>(particle_types_[type_id_[p1]].material)];
    const MaterialProperties& mat2 = material_properties_[static_cast<size_t>(particle_types_[type_id_[p2]].material)];
    
    // Harmonic mean of thermal conductivities
    return 2.0f * mat1.thermal_conductivity * mat2.thermal_conductivity / 
           (mat1.thermal_conductivity + mat2.thermal_conductivity);
}

float ParticleSystem::calculate_electrical_conductivity_between(uint32_t p1, uint32_t p2) const {
    if (p1 >= type_id_.size() || p2 >= type_id_.size() || !active_[p1] || !active_[p2]) {
        return 0.0f;
    }
    
    const MaterialProperties& mat1 = material_properties_[static_cast<size_t>(particle_types_[type_id_[p1]].material)];
    const MaterialProperties& mat2 = material_properties_[static_cast<size_t>(particle_types_[type_id_[p2]].material)];
    
    // Harmonic mean of electrical conductivities
    float cond1 = mat1.electrical_conductivity;
    float cond2 = mat2.electrical_conductivity;
    
    if (cond1 + cond2 == 0.0f) return 0.0f;
    
    return 2.0f * cond1 * cond2 / (cond1 + cond2);
}

bool ParticleSystem::can_react(const std::unordered_map<MaterialType, int>& reactants,
                              const std::vector<uint32_t>& nearby_particles) const {
    // Count available materials in nearby particles
    std::unordered_map<MaterialType, int> available_materials;
    
    for (uint32_t particle_idx : nearby_particles) {
        if (particle_idx < type_id_.size() && active_[particle_idx]) {
            MaterialType material = particle_types_[type_id_[particle_idx]].material;
            available_materials[material]++;
        }
    }
    
    // Check if we have enough reactants
    for (const auto& reactant : reactants) {
        MaterialType material = reactant.first;
        int required_count = reactant.second;
        
        auto it = available_materials.find(material);
        if (it == available_materials.end() || it->second < required_count) {
            return false;
        }
    }
    
    return true;
}

void ParticleSystem::consume_reactants(const std::unordered_map<MaterialType, int>& reactants,
                                      const std::vector<uint32_t>& nearby_particles) {
    // Count and mark particles for consumption
    std::unordered_map<MaterialType, int> to_consume = reactants;
    
    for (uint32_t particle_idx : nearby_particles) {
        if (particle_idx < type_id_.size() && active_[particle_idx]) {
            MaterialType material = particle_types_[type_id_[particle_idx]].material;
            
            auto it = to_consume.find(material);
            if (it != to_consume.end() && it->second > 0) {
                // Remove this particle
                remove_particle(particle_idx);
                it->second--;
                
                if (it->second == 0) {
                    to_consume.erase(it);
                }
                
                if (to_consume.empty()) {
                    break;
                }
            }
        }
    }
}

void ParticleSystem::spawn_products(const std::unordered_map<MaterialType, int>& products,
                                   const Vector3& reaction_center, float energy_change) {
    for (const auto& product : products) {
        MaterialType material = product.first;
        int count = product.second;
        
        // Find or create particle type for this material
        uint32_t product_type_id = 0;
        bool found_type = false;
        
        for (uint32_t i = 0; i < particle_types_.size(); ++i) {
            if (particle_types_[i].material == material) {
                product_type_id = i;
                found_type = true;
                break;
            }
        }
        
        if (!found_type) {
            // Create new particle type for this material
            const MaterialProperties& mat_props = material_properties_[static_cast<size_t>(material)];
            float radius = 0.3f; // Default radius
            float mass = mat_props.density * (4.0f/3.0f) * 3.14159f * powf(radius, 3); // Volume * density
            product_type_id = create_particle_type(radius, material, mass, mat_props.base_color);
        }
        
        // Spawn product particles
        for (int i = 0; i < count; ++i) {
            // Random position around reaction center
            Vector3 spawn_pos = {
                reaction_center.x + ((float)rand() / RAND_MAX - 0.5f) * 2.0f,
                reaction_center.y + ((float)rand() / RAND_MAX - 0.5f) * 2.0f,
                reaction_center.z + ((float)rand() / RAND_MAX - 0.5f) * 2.0f
            };
            
            // Random velocity
            Vector3 spawn_vel = {
                ((float)rand() / RAND_MAX - 0.5f) * 4.0f,
                ((float)rand() / RAND_MAX - 0.5f) * 4.0f,
                ((float)rand() / RAND_MAX - 0.5f) * 4.0f
            };
            
            // Temperature based on energy change
            float spawn_temp = 20.0f - (energy_change / 1000000.0f); // Convert J to reasonable temperature change
            spawn_temp = std::max(spawn_temp, -273.15f); // Don't go below absolute zero
            
            add_particle(product_type_id, spawn_pos, spawn_vel, spawn_temp, 0.0f);
        }
    }
}

// Debug visualization methods
void ParticleSystem::render_debug_spatial_info() {
    if (!debug_spatial_vis_ && !debug_neighbor_lines_ && 
        !debug_thermal_vis_ && !debug_electrical_vis_ && !debug_bonds_vis_) return;
    
    // Draw neighbor connections first (so they appear behind other elements)
    if (debug_neighbor_lines_) {
        PROFILE_SECTION("Draw Neighbor Lines");
        draw_neighbor_connections();
    }
    
    // Draw thermal visualization
    if (debug_thermal_vis_) {
        PROFILE_SECTION("Draw Thermal Visualization");
        draw_thermal_visualization();
    }
    
    // Draw electrical visualization
    if (debug_electrical_vis_) {
        PROFILE_SECTION("Draw Electrical Visualization");
        draw_electrical_visualization();
    }
    
    // Draw bonds visualization
    if (debug_bonds_vis_) {
        PROFILE_SECTION("Draw Bonds Visualization");
        draw_bonds_visualization();
    }
}

void ParticleSystem::draw_spatial_cell_boundaries(float x, float y, float z) {
    // Calculate which spatial cell this particle is in
    int cell_x = (int)floorf(x / SPATIAL_CELL_SIZE);
    int cell_y = (int)floorf(y / SPATIAL_CELL_SIZE);
    int cell_z = (int)floorf(z / SPATIAL_CELL_SIZE);
    
    // Calculate cell boundaries
    float min_x = cell_x * SPATIAL_CELL_SIZE;
    float min_y = cell_y * SPATIAL_CELL_SIZE;
    float min_z = cell_z * SPATIAL_CELL_SIZE;
    float max_x = (cell_x + 1) * SPATIAL_CELL_SIZE;
    float max_y = (cell_y + 1) * SPATIAL_CELL_SIZE;
    float max_z = (cell_z + 1) * SPATIAL_CELL_SIZE;
    
    // Draw wireframe cube for the spatial cell
    Color cell_color = {255, 255, 0, 100}; // Yellow, semi-transparent
    
    // Draw the 12 edges of the cube
    // Bottom face
    DrawLine3D({min_x, min_y, min_z}, {max_x, min_y, min_z}, cell_color);
    DrawLine3D({max_x, min_y, min_z}, {max_x, min_y, max_z}, cell_color);
    DrawLine3D({max_x, min_y, max_z}, {min_x, min_y, max_z}, cell_color);
    DrawLine3D({min_x, min_y, max_z}, {min_x, min_y, min_z}, cell_color);
    
    // Top face
    DrawLine3D({min_x, max_y, min_z}, {max_x, max_y, min_z}, cell_color);
    DrawLine3D({max_x, max_y, min_z}, {max_x, max_y, max_z}, cell_color);
    DrawLine3D({max_x, max_y, max_z}, {min_x, max_y, max_z}, cell_color);
    DrawLine3D({min_x, max_y, max_z}, {min_x, max_y, min_z}, cell_color);
    
    // Vertical edges
    DrawLine3D({min_x, min_y, min_z}, {min_x, max_y, min_z}, cell_color);
    DrawLine3D({max_x, min_y, min_z}, {max_x, max_y, min_z}, cell_color);
    DrawLine3D({max_x, min_y, max_z}, {max_x, max_y, max_z}, cell_color);
    DrawLine3D({min_x, min_y, max_z}, {min_x, max_y, max_z}, cell_color);
}

void ParticleSystem::draw_gravity_influence_sphere(float x, float y, float z, float radius, Color color) {
    // Draw wireframe sphere to show gravity influence radius
    Vector3 center = {x, y, z};
    
    // Draw sphere using line segments (more efficient than full sphere mesh)
    int rings = 2;
    int sectors = 12;
    
    for (int r = 0; r < rings; r++) { 
        float lat0 = PI * (-0.5f + (float)r / rings);
        float lat1 = PI * (-0.5f + (float)(r + 1) / rings);
        float y0 = sinf(lat0) * radius;
        float y1 = sinf(lat1) * radius;
        float r0 = cosf(lat0) * radius;  
        float r1 = cosf(lat1) * radius;
        
        for (int s = 0; s < sectors; s++) {
            float lng0 = 2 * PI * (float)s / sectors;
            float lng1 = 2 * PI * (float)(s + 1) / sectors;
            
            // Ring vertices
            Vector3 v0 = {x + cosf(lng0) * r0, y + y0, z + sinf(lng0) * r0};
            Vector3 v1 = {x + cosf(lng1) * r0, y + y0, z + sinf(lng1) * r0};
            Vector3 v2 = {x + cosf(lng0) * r1, y + y1, z + sinf(lng0) * r1};
            Vector3 v3 = {x + cosf(lng1) * r1, y + y1, z + sinf(lng1) * r1};
            
            // Draw ring segments
            DrawLine3D(v0, v1, color);
            DrawLine3D(v0, v2, color);
            
            if (r == rings - 1) {
                DrawLine3D(v2, v3, color);
            }
            if (s == sectors - 1) {
                DrawLine3D(v1, v3, color);
            }
        }
    }
}

void ParticleSystem::draw_neighbor_connections() {
    PROFILE_SECTION("Neighbor Connection Rendering");
    
    // Buffer for neighbor queries
    void* neighbors[MAX_NEIGHBORS];
    
    // Process each active particle to draw neighbor connections
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        const ParticleType& particle_type = particle_types_[type_id_[i]];
        
        // Calculate gravity radius for this particle type
        float gravity_radius = calculate_gravity_radius(particle_type.mass);
        
        float px = pos_x_[i];
        float py = pos_y_[i];
        float pz = pos_z_[i];
        Vector3 particle_pos = {px, py, pz};
        
        // Query neighbors within gravity influence radius (same as physics)
        int neighbor_count = sh_query_radius(spatial_hash_, px, py, pz, gravity_radius, neighbors, MAX_NEIGHBORS);
        
        // Draw lines to each neighbor that affects this particle's motion
        for (int n = 0; n < neighbor_count; ++n) {
            ParticleRef* neighbor_ref = (ParticleRef*)neighbors[n];
            uint32_t neighbor_idx = neighbor_ref->particle_index;
            
            // Skip self-reference
            if (neighbor_idx == i) continue;
            
            // Skip inactive neighbors
            if (neighbor_idx >= active_.size() || !active_[neighbor_idx]) continue;
            
            float nx = pos_x_[neighbor_idx];
            float ny = pos_y_[neighbor_idx];
            float nz = pos_z_[neighbor_idx];
            Vector3 neighbor_pos = {nx, ny, nz};
            
            // Calculate distance to determine line color and thickness
            float distance = Vector3Distance(particle_pos, neighbor_pos);
            
            // Only draw if distance is greater than minimum (same check as physics)
            if (distance > MIN_DISTANCE) {
                // Color based on distance - closer = brighter/thicker
                float influence_strength = 1.0f - (distance / gravity_radius);
                influence_strength = fmaxf(0.0f, fminf(1.0f, influence_strength));
                
                // Create color based on particle type and influence strength
                Color line_color = particle_type.color;
                line_color.a = (unsigned char)(influence_strength * 255 * 0.7f); // Semi-transparent based on strength
                
                // Make the line more visible
                line_color.r = (unsigned char)(line_color.r * 0.8f + 255 * 0.2f); // Brighten
                line_color.g = (unsigned char)(line_color.g * 0.8f + 255 * 0.2f);
                line_color.b = (unsigned char)(line_color.b * 0.8f + 255 * 0.2f);
                
                // Draw the connection line
                DrawLine3D(particle_pos, neighbor_pos, line_color);
                
                // For very close neighbors (high influence), draw a thicker line
                if (influence_strength > 0.7f) {
                    // Offset slightly for thickness effect
                    Vector3 offset1 = {particle_pos.x + 0.02f, particle_pos.y, particle_pos.z};
                    Vector3 offset2 = {neighbor_pos.x + 0.02f, neighbor_pos.y, neighbor_pos.z};
                    DrawLine3D(offset1, offset2, line_color);
                    
                    Vector3 offset3 = {particle_pos.x, particle_pos.y + 0.02f, particle_pos.z};
                    Vector3 offset4 = {neighbor_pos.x, neighbor_pos.y + 0.02f, neighbor_pos.z};
                    DrawLine3D(offset3, offset4, line_color);
                }
            }
        }
    }
}

void ParticleSystem::draw_thermal_visualization() {
    // Draw temperature as colored halos around particles
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        const ParticleType& type = particle_types_[type_id_[i]];
        Vector3 pos = {pos_x_[i], pos_y_[i], pos_z_[i]};
        float temp = temperature_[i];
        
        // Color based on temperature
        Color temp_color;
        if (temp < 0.0f) {
            // Cold - blue
            temp_color = Color{0, 100, 255, 50};
        } else if (temp < 100.0f) {
            // Cool - green to yellow
            float factor = temp / 100.0f;
            temp_color = Color{static_cast<unsigned char>(factor * 255), 255, 0, 50};
        } else if (temp < 500.0f) {
            // Warm - orange to red
            float factor = (temp - 100.0f) / 400.0f;
            temp_color = Color{255, static_cast<unsigned char>((1.0f - factor) * 255), 0, 50};
        } else {
            // Hot - red to white
            float factor = std::min(1.0f, (temp - 500.0f) / 500.0f);
            temp_color = Color{255, static_cast<unsigned char>(factor * 255), static_cast<unsigned char>(factor * 255), 80};
        }
        
        // Draw thermal halo
        float halo_radius = type.radius * 4.0f;
        DrawSphereWires(pos, halo_radius, 8, 8, temp_color);
    }
}

void ParticleSystem::draw_electrical_visualization() {
    // Draw electrical charges and connections
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        Vector3 pos = {pos_x_[i], pos_y_[i], pos_z_[i]};
        float charge = charge_[i];
        float voltage = voltage_[i];
        
        if (std::abs(charge) > 0.01f) {
            // Color based on charge
            Color charge_color;
            if (charge > 0.0f) {
                // Positive charge - red
                float intensity = std::min(1.0f, charge / 10.0f);
                charge_color = Color{255, 0, 0, static_cast<unsigned char>(intensity * 100)};
            } else {
                // Negative charge - blue
                float intensity = std::min(1.0f, -charge / 10.0f);
                charge_color = Color{0, 0, 255, static_cast<unsigned char>(intensity * 100)};
            }
            
            // Draw charge visualization
            const ParticleType& type = particle_types_[type_id_[i]];
            float charge_radius = type.radius * 3.0f;
            DrawSphereWires(pos, charge_radius, 6, 6, charge_color);
            
            // Draw voltage level as vertical line
            if (std::abs(voltage) > 0.1f) {
                Vector3 voltage_end = {pos.x, pos.y + voltage * 0.1f, pos.z};
                DrawLine3D(pos, voltage_end, charge_color);
            }
        }
    }
}

void ParticleSystem::draw_bonds_visualization() {
    // Draw bonds between particles
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        Vector3 pos = {pos_x_[i], pos_y_[i], pos_z_[i]};
        
        for (const auto& bond : bonds_[i]) {
            uint32_t bonded_idx = bond.particle_index;
            
            if (bonded_idx >= active_.size() || !active_[bonded_idx] || bonded_idx <= i) {
                continue; // Skip invalid bonds or draw each bond only once
            }
            
            Vector3 bonded_pos = {pos_x_[bonded_idx], pos_y_[bonded_idx], pos_z_[bonded_idx]};
            
            // Color based on bond strength
            float strength_factor = std::min(1.0f, bond.strength / 20.0f);
            Color bond_color = Color{
                static_cast<unsigned char>(255 * strength_factor),
                static_cast<unsigned char>(255 * (1.0f - strength_factor)),
                0,
                150
            };
            
            // Draw bond as line
            DrawLine3D(pos, bonded_pos, bond_color);
            
            // Draw bond strength as cylinder thickness (multiple lines for thick bonds)
            if (bond.strength > 10.0f) {
                Vector3 offset1 = {pos.x + 0.05f, pos.y, pos.z};
                Vector3 offset2 = {bonded_pos.x + 0.05f, bonded_pos.y, bonded_pos.z};
                DrawLine3D(offset1, offset2, bond_color);
                
                Vector3 offset3 = {pos.x, pos.y + 0.05f, pos.z};
                Vector3 offset4 = {bonded_pos.x, bonded_pos.y + 0.05f, bonded_pos.z};
                DrawLine3D(offset3, offset4, bond_color);
            }
        }
    }
}

// Profiling interface methods
void ParticleSystem::print_profiling_stats() const {
    Performance::Profiler::instance().print_stats();
}

void ParticleSystem::reset_profiling_stats() {
    Performance::Profiler::instance().reset_stats();
}

double ParticleSystem::get_profiling_section_time(const std::string& section) const {
    return Performance::Profiler::instance().get_section_time_ms(section);
}

// Instanced rendering implementation
void ParticleSystem::initialize_instanced_rendering() {
    printf("Initializing instanced particle rendering...\n");
    
    try {
        // Create a high-quality sphere mesh for particles
        sphere_mesh_ = GenMeshSphere(1.0f, 16, 16); // Unit sphere, 16x16 resolution
        
        // Create default material for particles
        particle_material_ = LoadMaterialDefault();
        
        // Reserve space for instance data
        instance_buffer_.reserve(10000); // Reserve for many particles
        
        instanced_rendering_initialized_ = true;
        printf("  ✓ Instanced rendering initialized successfully\n");
        printf("  ✓ Sphere mesh: %d vertices, %d triangles\n", 
               sphere_mesh_.vertexCount, sphere_mesh_.triangleCount);
        
    } catch (...) {
        printf("  ✗ Failed to initialize instanced rendering - using fallback\n");
        instanced_rendering_initialized_ = false;
    }
}

void ParticleSystem::cleanup_instanced_rendering() {
    if (instanced_rendering_initialized_) {
        printf("Cleaning up instanced rendering resources...\n");
        
        // Unload mesh and material
        UnloadMesh(sphere_mesh_);
        UnloadMaterial(particle_material_);
        
        // Clear instance buffer
        instance_buffer_.clear();
        
        instanced_rendering_initialized_ = false;
        printf("  ✓ Instanced rendering cleanup complete\n");
    }
}

void ParticleSystem::render_particles_instanced() {
    if (!instanced_rendering_initialized_ || pos_x_.empty()) {
        return;
    }
    
    // Collect all particle instance data
    {
        PROFILE_SECTION("Collect Instance Data");
        collect_instance_data();
    }
    
    if (instance_buffer_.empty()) {
        return; // No particles to render
    }
    
    // Use optimized rendering approaches
    {
        PROFILE_SECTION("GPU Instanced Draw");
        
        // Method 1: Batched sphere rendering (more efficient than individual DrawSphere calls)
        // Group particles by similar size to reduce state changes
        std::sort(instance_buffer_.begin(), instance_buffer_.end(), 
                  [](const ParticleInstanceData& a, const ParticleInstanceData& b) {
                      return a.radius < b.radius;
                  });
        
        // Batch render with shared mesh but different transforms
        for (const auto& instance : instance_buffer_) {
            // Use efficient matrix-based rendering
            Matrix transform = MatrixScale(instance.radius, instance.radius, instance.radius);
            transform = MatrixMultiply(transform, MatrixTranslate(instance.position.x, instance.position.y, instance.position.z));
            
            // Set material color (this is still efficient)
            particle_material_.maps[MATERIAL_MAP_DIFFUSE].color = instance.color;
            
            // Draw with pre-built mesh and transformation matrix
            DrawMesh(sphere_mesh_, particle_material_, transform);
        }
        
        // Optional: Add wireframe overlay for better visual definition
        // (Only for particles that are large enough to benefit from it)
        {
            PROFILE_SECTION("Wireframe Overlay");
            for (const auto& instance : instance_buffer_) {
                if (instance.radius > 0.3f) { // Only for larger particles
                    Color wireframe_color = instance.color;
                    wireframe_color.a = 64; // Semi-transparent
                    DrawSphereWires(instance.position, instance.radius, 8, 8, wireframe_color);
                }
            }
        }
    }
}

void ParticleSystem::collect_instance_data() {
    instance_buffer_.clear();
    
    // Collect data from all active particles
    for (uint32_t i = 0; i < pos_x_.size(); ++i) {
        if (!active_[i]) continue;
        
        const ParticleType& type = particle_types_[type_id_[i]];
        
        ParticleInstanceData instance;
        
        // Position
        instance.position = {pos_x_[i], pos_y_[i], pos_z_[i]};
        
        // Visual radius based on phase state and material properties
        float visual_radius = type.radius;
        
        // Adjust radius based on phase
        switch (phase_state_[i]) {
            case PhaseState::Gas:
                visual_radius *= 1.5f; // Gases are more diffuse
                break;
            case PhaseState::Plasma:
                visual_radius *= 2.0f; // Plasma is very diffuse
                break;
            case PhaseState::Liquid:
                visual_radius *= 1.1f; // Liquids slightly larger
                break;
            case PhaseState::Solid:
            default:
                // Keep original radius
                break;
        }
        
        // Amplify for better visibility
        instance.radius = visual_radius * (2.0f + type.mass * 0.1f);
        
        // Color based on material properties, temperature, and phase
        instance.color = get_material_color(i);
        
        instance_buffer_.push_back(instance);
    }
}

// Rendering mode control methods
void ParticleSystem::cycle_rendering_mode() {
    use_instanced_rendering_ = !use_instanced_rendering_;
    printf("Rendering mode changed to: %s\n", get_rendering_mode_name());
}

const char* ParticleSystem::get_rendering_mode_name() const {
    if (use_instanced_rendering_ && instanced_rendering_initialized_) {
        return "Instanced (Optimized)";
    } else if (use_instanced_rendering_ && !instanced_rendering_initialized_) {
        return "Instanced (Failed - using Individual)";
    } else {
        return "Individual (Legacy)";
    }
} 