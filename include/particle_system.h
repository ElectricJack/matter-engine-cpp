#pragma once

extern "C" {
    #include "raylib.h"
}
#include "raymath.h"

#include <vector>
#include <memory>
#include <ode/ode.h>

struct Particle {
    dBodyID body;
    dGeomID geom;
    float radius;
    Color color;
    bool active;
    
    Particle() : body(nullptr), geom(nullptr), radius(0.5f), color(WHITE), active(false) {}
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
    void add_particle(const Vector3& position, const Vector3& velocity, float mass = 1.0f, float radius = 0.5f);
    void remove_particle(int index);
    
    // Simulation
    void update(float dt);
    void render();
    
    // Info
    int get_particle_count() const;
    float get_physics_time_ms() const;
    
private:
    // ODE physics world
    dWorldID world_;
    dSpaceID space_;
    dJointGroupID contact_group_;
    
    // Particles
    std::vector<std::unique_ptr<Particle>> particles_;
    
    // Ground plane
    dGeomID ground_geom_;
    
    // Performance tracking
    float physics_time_ms_;
    
    // Physics parameters
    static constexpr float GRAVITY = -9.81f;
    static constexpr float BOUNCE_FACTOR = 0.8f;
    static constexpr float FRICTION = 0.1f;
    static constexpr int MAX_CONTACTS = 10;
    
    // Internal methods
    void create_ground_plane();
    void step_physics(float dt);
    static void collision_callback(void* data, dGeomID o1, dGeomID o2);
    void handle_collision(dGeomID o1, dGeomID o2);
    
    // Rendering helpers
    void render_particle(const Particle& particle);
    Vector3 get_particle_position(const Particle& particle);
    Color get_random_particle_color();
}; 