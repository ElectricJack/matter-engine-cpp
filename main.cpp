extern "C" {
    #include "raylib.h"
    #include "rlgl.h"
}
#include "raymath.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#include <ode/ode.h>

#include "particle_system.h"

class ParticleDynamicsDemo {
public:
    ParticleDynamicsDemo(int width, int height, bool debug_mode = false) 
        : screen_width_(width), screen_height_(height),
          debug_mode_(debug_mode), debug_frame_count_(0),
          particle_system_(std::make_unique<ParticleSystem>()) {
        
        InitWindow(screen_width_, screen_height_, "Particle Dynamics with ODE Physics");
        SetTargetFPS(60);
        
        // Disable cursor for first person camera control
        DisableCursor();
        
        setup_scene();
        
        printf("=== Particle Dynamics System Initialized ===\n");
        printf("ODE version: %s\n", dGetConfiguration());
    }
    
    ~ParticleDynamicsDemo() {
        cleanup();
        EnableCursor(); // Restore cursor functionality before closing
        CloseWindow();
    }
    
    void run() {
        int frame_count = 0;
        
        if (debug_mode_) {
            printf("=== DEBUG MODE: Will auto-quit after 300 frames ===\n");
        }
        
        while (!WindowShouldClose()) {
            frame_count++;
            debug_frame_count_ = frame_count;
            
            // Debug mode auto-quit
            if (debug_mode_ && frame_count >= 300) {
                printf("DEBUG MODE: Reached 300 frames, auto-quitting...\n");
                break;
            }
            
            // Print progress every 60 frames
            if (frame_count % 60 == 0) {
                printf("Frame %d - Particles: %d\n", frame_count, particle_system_->get_particle_count());
            }
            
            update();
            render();
        }
        
        // Final stats
        printf("\n=== Final Statistics ===\n");
        printf("Total frames: %d\n", frame_count);
        printf("Final particle count: %d\n", particle_system_->get_particle_count());
    }

private:
    void setup_scene() {
        printf("=== Setting Up Particle Physics Scene ===\n");
        
        // Initialize camera
        camera_.position = {0.0f, 10.0f, 20.0f};
        camera_.target = {0.0f, 0.0f, 0.0f};
        camera_.up = {0.0f, 1.0f, 0.0f};
        camera_.fovy = 45.0f;
        camera_.projection = CAMERA_PERSPECTIVE;
        
        // Initialize particle system
        particle_system_->initialize();
        
        // Add some initial particles
        for (int i = 0; i < 20; i++) {
            float x = (i % 5 - 2) * 2.0f;
            float z = (i / 5 - 2) * 2.0f;
            float y = 10.0f + i * 0.5f;
            
            particle_system_->add_particle(Vector3{x, y, z}, Vector3{0, 0, 0}, 1.0f, 0.5f);
        }
        
        printf("Scene setup complete!\n");
    }
    
    void update() {
        // Handle input
        handle_input();
        
        // Update physics
        float dt = GetFrameTime();
        particle_system_->update(dt);
        
        // Update camera (DisableCursor handles mouse capture automatically)
        UpdateCamera(&camera_, CAMERA_FREE);
    }
    
    void handle_input() {
        // Toggle cursor mode
        if (IsKeyPressed(KEY_ESCAPE)) {
            cursor_disabled_ = !cursor_disabled_;
            if (cursor_disabled_) {
                DisableCursor();
                printf("Mouse captured for first person camera control\n");
            } else {
                EnableCursor();
                printf("Mouse cursor released\n");
            }
        }
        
        // Add particle at camera position
        if (IsKeyPressed(KEY_SPACE)) {
            Vector3 pos = camera_.position;
            Vector3 vel = Vector3Scale(Vector3Normalize(Vector3Subtract(camera_.target, camera_.position)), 10.0f);
            particle_system_->add_particle(pos, vel, 1.0f, 0.3f);
            printf("Added particle at camera position\n");
        }
        
        // Reset simulation
        if (IsKeyPressed(KEY_R)) {
            printf("Resetting simulation...\n");
            particle_system_->reset();
            setup_scene();
        }
        
        // Add explosion of particles
        if (IsKeyPressed(KEY_E)) {
            printf("Creating particle explosion...\n");
            for (int i = 0; i < 50; i++) {
                float angle1 = (float)i / 50.0f * 2.0f * PI;
                float angle2 = ((float)rand() / RAND_MAX - 0.5f) * PI;
                
                Vector3 pos = {0, 5, 0};
                Vector3 vel = {
                    cosf(angle1) * cosf(angle2) * 15.0f,
                    sinf(angle2) * 15.0f + 5.0f,
                    sinf(angle1) * cosf(angle2) * 15.0f
                };
                
                particle_system_->add_particle(pos, vel, 0.5f, 0.2f);
            }
        }
    }
    
    void render() {
        BeginDrawing();
        ClearBackground(BLACK);
        
        BeginMode3D(camera_);
        
        // Draw ground plane
        DrawPlane(Vector3{0, 0, 0}, Vector2{100, 100}, DARKGRAY);
        DrawGrid(20, 1.0f);
        
        // Draw coordinate axes
        DrawLine3D(Vector3{0, 0, 0}, Vector3{5, 0, 0}, RED);
        DrawLine3D(Vector3{0, 0, 0}, Vector3{0, 5, 0}, GREEN);
        DrawLine3D(Vector3{0, 0, 0}, Vector3{0, 0, 5}, BLUE);
        
        // Render particles
        particle_system_->render();
        
        EndMode3D();
        
        render_ui();
        
        EndDrawing();
    }
    
    void render_ui() {
        // System info
        DrawText("PARTICLE DYNAMICS DEMO", 10, 10, 20, GREEN);
        DrawText(TextFormat("Particles: %d", particle_system_->get_particle_count()), 10, 40, 16, WHITE);
        DrawText(TextFormat("FPS: %d", GetFPS()), 10, 60, 16, WHITE);
        
        // Controls
        DrawText("Controls:", 10, 100, 16, YELLOW);
        DrawText("SPACE - Add particle at camera", 10, 120, 14, LIGHTGRAY);
        DrawText("E - Particle explosion", 10, 140, 14, LIGHTGRAY);
        DrawText("R - Reset simulation", 10, 160, 14, LIGHTGRAY);
        DrawText("ESC - Toggle mouse capture", 10, 180, 14, LIGHTGRAY);
        DrawText("WASD/Mouse - Camera control", 10, 200, 14, LIGHTGRAY);
        
        // Physics info
        DrawText(TextFormat("Physics step: %.2f ms", particle_system_->get_physics_time_ms()), 10, 240, 14, LIME);
        
        // Mouse control info
        if (cursor_disabled_) {
            DrawText("Mouse captured for camera control (ESC to release)", 10, screen_height_ - 30, 14, LIGHTGRAY);
        } else {
            DrawText("Mouse cursor free (ESC to capture for camera control)", 10, screen_height_ - 30, 14, YELLOW);
        }
    }
    
    void cleanup() {
        // Particle system cleanup is handled by destructor
    }
    
private:
    // Window settings
    int screen_width_;
    int screen_height_;
    
    // Debug mode
    bool debug_mode_;
    int debug_frame_count_;
    
    // Systems
    std::unique_ptr<ParticleSystem> particle_system_;
    
    // Rendering
    Camera camera_;
    bool cursor_disabled_ = true;
};

int main(int argc, char* argv[]) {
    printf("=== Particle Dynamics Demo with ODE Physics ===\n");
    
    bool debug_mode = false;
    
    // Check for debug flag
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
            debug_mode = true;
            printf("DEBUG MODE ENABLED: Will auto-quit after 300 frames\n");
        }
    }
    
    try {
        ParticleDynamicsDemo demo(1280, 800, debug_mode);
        demo.run();
    } catch (const std::exception& e) {
        printf("Error: %s\n", e.what());
        return 1;
    }
    
    return 0;
} 