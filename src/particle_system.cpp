#include "particle_system.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <algorithm>

ParticleSystem::ParticleSystem() 
    : world_(nullptr), space_(nullptr), contact_group_(nullptr), 
      ground_geom_(nullptr), physics_time_ms_(0.0f) {
    srand(static_cast<unsigned int>(time(nullptr)));
}

ParticleSystem::~ParticleSystem() {
    cleanup();
}

void ParticleSystem::initialize() {
    printf("Initializing ODE physics world...\n");
    
    // Initialize ODE
    dInitODE();
    
    // Create world
    world_ = dWorldCreate();
    dWorldSetGravity(world_, 0, GRAVITY, 0);
    dWorldSetCFM(world_, 1e-4);  // Less stiff
    dWorldSetERP(world_, 0.8);   // Higher error reduction
    dWorldSetContactMaxCorrectingVel(world_, 0.1);  // Lower max velocity
    dWorldSetContactSurfaceLayer(world_, 0.01);     // Thicker surface layer
    
    // Use simple space instead of hash space to avoid bounds issues
    space_ = dSimpleSpaceCreate(0);
    
    // Create contact joint group
    contact_group_ = dJointGroupCreate(0);
    
    // Create ground plane
    create_ground_plane();
    
    printf("ODE physics world initialized successfully!\n");
    printf("  World ID: %p\n", world_);
    printf("  Space ID: %p\n", space_);
    printf("  Gravity: %.2f\n", GRAVITY);
}

void ParticleSystem::cleanup() {
    if (!world_) return;
    
    printf("Cleaning up physics world...\n");
    
    // Clean up particles
    particles_.clear();
    
    // Clean up ground
    if (ground_geom_) {
        dGeomDestroy(ground_geom_);
        ground_geom_ = nullptr;
    }
    
    // Clean up ODE objects
    if (contact_group_) {
        dJointGroupDestroy(contact_group_);
        contact_group_ = nullptr;
    }
    
    if (space_) {
        dSpaceDestroy(space_);
        space_ = nullptr;
    }
    
    if (world_) {
        dWorldDestroy(world_);
        world_ = nullptr;
    }
    
    // Close ODE
    dCloseODE();
    
    printf("Physics world cleanup complete.\n");
}

void ParticleSystem::reset() {
    printf("Resetting particle system...\n");
    
    // Remove all particles
    particles_.clear();
    
    // Reset joint group
    if (contact_group_) {
        dJointGroupEmpty(contact_group_);
    }
    
    printf("Particle system reset complete.\n");
}

void ParticleSystem::create_ground_plane() {
    // Create infinite ground plane at y=0
    ground_geom_ = dCreatePlane(space_, 0, 1, 0, 0);
    printf("Ground plane created.\n");
}

void ParticleSystem::add_particle(const Vector3& position, const Vector3& velocity, float mass, float radius) {
    if (!world_ || !space_) {
        printf("Error: Physics world not initialized!\n");
        return;
    }
    
    auto particle = std::make_unique<Particle>();
    particle->radius = radius;
    particle->color = get_random_particle_color();
    particle->active = true;
    
    // Create rigid body
    particle->body = dBodyCreate(world_);
    dBodySetPosition(particle->body, position.x, position.y, position.z);
    dBodySetLinearVel(particle->body, velocity.x, velocity.y, velocity.z);
    
    // Set mass
    dMass m;
    dMassSetSphere(&m, 1.0, radius);
    dMassAdjust(&m, mass);
    dBodySetMass(particle->body, &m);
    
    // Create collision geometry
    particle->geom = dCreateSphere(space_, radius);
    dGeomSetBody(particle->geom, particle->body);
    
    // Store particle pointer in geom for collision callback
    dGeomSetData(particle->geom, particle.get());
    
    particles_.push_back(std::move(particle));
    
    printf("Added particle %zu at (%.2f, %.2f, %.2f) with velocity (%.2f, %.2f, %.2f)\n",
           particles_.size() - 1, position.x, position.y, position.z,
           velocity.x, velocity.y, velocity.z);
}

void ParticleSystem::remove_particle(int index) {
    if (index < 0 || index >= static_cast<int>(particles_.size())) {
        return;
    }
    
    particles_.erase(particles_.begin() + index);
    printf("Removed particle %d\n", index);
}

void ParticleSystem::update(float dt) {
    if (!world_) return;
    
    // Measure physics step time
    double start_time = GetTime();
    
    step_physics(dt);
    
    double end_time = GetTime();
    physics_time_ms_ = static_cast<float>((end_time - start_time) * 1000.0);
}

void ParticleSystem::step_physics(float dt) {
    // Limit time step for stability
    const float max_dt = 1.0f / 60.0f;
    dt = std::min(dt, max_dt);
    
    // Check for runaway particles and clamp positions
    for (auto& particle : particles_) {
        if (particle->body) {
            const dReal* pos = dBodyGetPosition(particle->body);
            const float max_pos = 100.0f;  // Reasonable bounds
            
            if (std::abs(pos[0]) > max_pos || std::abs(pos[1]) > max_pos || std::abs(pos[2]) > max_pos) {
                // Reset runaway particle
                dBodySetPosition(particle->body, 0, 10, 0);
                dBodySetLinearVel(particle->body, 0, 0, 0);
                dBodySetAngularVel(particle->body, 0, 0, 0);
            }
        }
    }
    
    // Collision detection
    dSpaceCollide(space_, this, &collision_callback);
    
    // Step the world
    dWorldStep(world_, dt);
    
    // Remove all contact joints
    dJointGroupEmpty(contact_group_);
}

void ParticleSystem::collision_callback(void* data, dGeomID o1, dGeomID o2) {
    ParticleSystem* system = static_cast<ParticleSystem*>(data);
    system->handle_collision(o1, o2);
}

void ParticleSystem::handle_collision(dGeomID o1, dGeomID o2) {
    dContact contact[MAX_CONTACTS];
    
    int num_contacts = dCollide(o1, o2, MAX_CONTACTS, &contact[0].geom, sizeof(dContact));
    
    for (int i = 0; i < num_contacts; i++) {
        contact[i].surface.mode = dContactBounce | dContactSoftCFM;
        contact[i].surface.mu = FRICTION;
        contact[i].surface.bounce = BOUNCE_FACTOR;
        contact[i].surface.bounce_vel = 0.1;
        contact[i].surface.soft_cfm = 0.001;
        
        dJointID c = dJointCreateContact(world_, contact_group_, &contact[i]);
        dJointAttach(c, dGeomGetBody(o1), dGeomGetBody(o2));
    }
}

void ParticleSystem::render() {
    // Render all active particles
    for (const auto& particle : particles_) {
        if (particle->active) {
            render_particle(*particle);
        }
    }
}

void ParticleSystem::render_particle(const Particle& particle) {
    Vector3 pos = get_particle_position(particle);
    
    // Draw sphere
    DrawSphere(pos, particle.radius, particle.color);
    
    // Draw wireframe for better visibility
    DrawSphereWires(pos, particle.radius, 8, 8, 
                    Color{particle.color.r, particle.color.g, particle.color.b, 128});
}

Vector3 ParticleSystem::get_particle_position(const Particle& particle) {
    if (!particle.body) return {0, 0, 0};
    
    const dReal* pos = dBodyGetPosition(particle.body);
    return {static_cast<float>(pos[0]), static_cast<float>(pos[1]), static_cast<float>(pos[2])};
}

Color ParticleSystem::get_random_particle_color() {
    Color colors[] = {
        RED, GREEN, BLUE, YELLOW, MAGENTA, ORANGE, 
        PURPLE, LIME, PINK, SKYBLUE, VIOLET, GOLD
    };
    
    int color_count = sizeof(colors) / sizeof(colors[0]);
    return colors[rand() % color_count];
}

int ParticleSystem::get_particle_count() const {
    return static_cast<int>(particles_.size());
}

float ParticleSystem::get_physics_time_ms() const {
    return physics_time_ms_;
} 