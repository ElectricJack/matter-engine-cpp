#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/bvh.h"
#include "include/blas_manager.h"
#include "include/tlas_manager.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper function to create triangle from raylib-style vertices
Triangle create_triangle(Vector3 v0, Vector3 v1, Vector3 v2, int material_id) {
    Triangle tri;
    tri.v0 = (Vec3){v0.x, v0.y, v0.z};
    tri.v1 = (Vec3){v1.x, v1.y, v1.z};
    tri.v2 = (Vec3){v2.x, v2.y, v2.z};
    
    // Calculate centroid
    tri.centroid.x = (tri.v0.x + tri.v1.x + tri.v2.x) / 3.0f;
    tri.centroid.y = (tri.v0.y + tri.v1.y + tri.v2.y) / 3.0f;
    tri.centroid.z = (tri.v0.z + tri.v1.z + tri.v2.z) / 3.0f;
    
    // Calculate normal using cross product
    Vec3 edge1 = {tri.v1.x - tri.v0.x, tri.v1.y - tri.v0.y, tri.v1.z - tri.v0.z};
    Vec3 edge2 = {tri.v2.x - tri.v0.x, tri.v2.y - tri.v0.y, tri.v2.z - tri.v0.z};
    
    tri.normal.x = edge1.y * edge2.z - edge1.z * edge2.y;
    tri.normal.y = edge1.z * edge2.x - edge1.x * edge2.z;
    tri.normal.z = edge1.x * edge2.y - edge1.y * edge2.x;
    
    // Normalize
    float len = sqrtf(tri.normal.x * tri.normal.x + tri.normal.y * tri.normal.y + tri.normal.z * tri.normal.z);
    if (len > 0.0f) {
        tri.normal.x /= len;
        tri.normal.y /= len;
        tri.normal.z /= len;
    }
    
    tri.material_id = material_id;
    return tri;
}

// Create cube triangles
Triangle* create_cube_triangles(int* triangle_count) {
    *triangle_count = 12;
    Triangle* triangles = malloc(12 * sizeof(Triangle));
    if (!triangles) return NULL;
    
    int tri_idx = 0;
    
    // Front face (Z+)
    triangles[tri_idx++] = create_triangle((Vector3){-0.5f, -0.5f, 0.5f}, (Vector3){0.5f, -0.5f, 0.5f}, (Vector3){0.5f, 0.5f, 0.5f}, 0);
    triangles[tri_idx++] = create_triangle((Vector3){-0.5f, -0.5f, 0.5f}, (Vector3){0.5f, 0.5f, 0.5f}, (Vector3){-0.5f, 0.5f, 0.5f}, 0);
    
    // Back face (Z-)
    triangles[tri_idx++] = create_triangle((Vector3){-0.5f, -0.5f, -0.5f}, (Vector3){0.5f, 0.5f, -0.5f}, (Vector3){0.5f, -0.5f, -0.5f}, 0);
    triangles[tri_idx++] = create_triangle((Vector3){-0.5f, -0.5f, -0.5f}, (Vector3){-0.5f, 0.5f, -0.5f}, (Vector3){0.5f, 0.5f, -0.5f}, 0);
    
    // Right face (X+)
    triangles[tri_idx++] = create_triangle((Vector3){0.5f, -0.5f, -0.5f}, (Vector3){0.5f, 0.5f, -0.5f}, (Vector3){0.5f, 0.5f, 0.5f}, 0);
    triangles[tri_idx++] = create_triangle((Vector3){0.5f, -0.5f, -0.5f}, (Vector3){0.5f, 0.5f, 0.5f}, (Vector3){0.5f, -0.5f, 0.5f}, 0);
    
    // Left face (X-)
    triangles[tri_idx++] = create_triangle((Vector3){-0.5f, -0.5f, -0.5f}, (Vector3){-0.5f, 0.5f, 0.5f}, (Vector3){-0.5f, 0.5f, -0.5f}, 0);
    triangles[tri_idx++] = create_triangle((Vector3){-0.5f, -0.5f, -0.5f}, (Vector3){-0.5f, -0.5f, 0.5f}, (Vector3){-0.5f, 0.5f, 0.5f}, 0);
    
    // Top face (Y+)
    triangles[tri_idx++] = create_triangle((Vector3){-0.5f, 0.5f, -0.5f}, (Vector3){-0.5f, 0.5f, 0.5f}, (Vector3){0.5f, 0.5f, 0.5f}, 0);
    triangles[tri_idx++] = create_triangle((Vector3){-0.5f, 0.5f, -0.5f}, (Vector3){0.5f, 0.5f, 0.5f}, (Vector3){0.5f, 0.5f, -0.5f}, 0);
    
    // Bottom face (Y-)
    triangles[tri_idx++] = create_triangle((Vector3){-0.5f, -0.5f, -0.5f}, (Vector3){0.5f, -0.5f, 0.5f}, (Vector3){-0.5f, -0.5f, 0.5f}, 0);
    triangles[tri_idx++] = create_triangle((Vector3){-0.5f, -0.5f, -0.5f}, (Vector3){0.5f, -0.5f, -0.5f}, (Vector3){0.5f, -0.5f, 0.5f}, 0);
    
    return triangles;
}

// Create sphere triangles
Triangle* create_sphere_triangles(float radius, int segments, int rings, int* triangle_count) {
    *triangle_count = 2 * segments * rings;
    Triangle* triangles = malloc(*triangle_count * sizeof(Triangle));
    if (!triangles) return NULL;
    
    int tri_idx = 0;
    
    for (int ring = 0; ring < rings; ring++) {
        for (int segment = 0; segment < segments; segment++) {
            // Calculate angles
            float ring_angle_1 = (float)ring / (float)rings * M_PI;
            float ring_angle_2 = (float)(ring + 1) / (float)rings * M_PI;
            float seg_angle_1 = (float)segment / (float)segments * 2.0f * M_PI;
            float seg_angle_2 = (float)(segment + 1) / (float)segments * 2.0f * M_PI;
            
            // Calculate vertices
            Vector3 v1 = {
                radius * sinf(ring_angle_1) * cosf(seg_angle_1),
                radius * cosf(ring_angle_1),
                radius * sinf(ring_angle_1) * sinf(seg_angle_1)
            };
            Vector3 v2 = {
                radius * sinf(ring_angle_1) * cosf(seg_angle_2),
                radius * cosf(ring_angle_1),
                radius * sinf(ring_angle_1) * sinf(seg_angle_2)
            };
            Vector3 v3 = {
                radius * sinf(ring_angle_2) * cosf(seg_angle_1),
                radius * cosf(ring_angle_2),
                radius * sinf(ring_angle_2) * sinf(seg_angle_1)
            };
            Vector3 v4 = {
                radius * sinf(ring_angle_2) * cosf(seg_angle_2),
                radius * cosf(ring_angle_2),
                radius * sinf(ring_angle_2) * sinf(seg_angle_2)
            };
            
            // Create two triangles for this quad (skip degenerate triangles)
            if (ring < rings - 1) {
                triangles[tri_idx++] = create_triangle(v1, v2, v3, 1);
                triangles[tri_idx++] = create_triangle(v2, v4, v3, 1);
            }
        }
    }
    
    *triangle_count = tri_idx; // Update to actual count
    return triangles;
}

// Create ground plane triangles
Triangle* create_ground_triangles(int* triangle_count) {
    *triangle_count = 2;
    Triangle* triangles = malloc(2 * sizeof(Triangle));
    if (!triangles) return NULL;
    
    float size = 10.0f;
    triangles[0] = create_triangle(
        (Vector3){-size, -1.0f, -size}, 
        (Vector3){size, -1.0f, -size}, 
        (Vector3){size, -1.0f, size}, 2);
    triangles[1] = create_triangle(
        (Vector3){-size, -1.0f, -size}, 
        (Vector3){size, -1.0f, size}, 
        (Vector3){-size, -1.0f, size}, 2);
    
    return triangles;
}

// Scene setup using modular system
void setup_scene(BLASManager* blas_manager, TLASManager* tlas_manager) {
    // Register different geometry types with BLAS manager
    int triangle_count;
    Triangle* triangles;
    
    // Register cube geometry
    triangles = create_cube_triangles(&triangle_count);
    BLASHandle cube_blas = blas_manager_register_triangles(blas_manager, triangles, triangle_count, 4);
    free(triangles);
    
    // Register sphere geometry  
    triangles = create_sphere_triangles(0.5f, 32, 16, &triangle_count);
    BLASHandle sphere_blas = blas_manager_register_triangles(blas_manager, triangles, triangle_count, 4);
    free(triangles);
    
    // Register ground geometry
    triangles = create_ground_triangles(&triangle_count);
    BLASHandle ground_blas = blas_manager_register_triangles(blas_manager, triangles, triangle_count, 2);
    free(triangles);
    
    printf("Registered BLAS handles: cube=%u, sphere=%u, ground=%u\n", 
           cube_blas, sphere_blas, ground_blas);
    
    // Clear any existing scene
    tlas_manager_clear(tlas_manager);
    
    // Build scene using matrix stack operations
    
    // Ground plane
    tlas_manager_load_identity(tlas_manager);
    tlas_manager_draw(tlas_manager, ground_blas, 2);
    
    // Central cube
    tlas_manager_load_identity(tlas_manager);
    tlas_manager_draw(tlas_manager, cube_blas, 0);
    
    // Transformed cube
    tlas_manager_load_identity(tlas_manager);
    tlas_manager_translate(tlas_manager, -3.0f, 0.0f, 0.0f);
    tlas_manager_rotate_y(tlas_manager, M_PI / 3.0f);
    tlas_manager_scale(tlas_manager, 0.8f, 1.2f, 0.8f);
    tlas_manager_draw(tlas_manager, cube_blas, 1);
    
    // Elevated cube with complex transform
    tlas_manager_load_identity(tlas_manager);
    tlas_manager_translate(tlas_manager, 3.0f, 2.0f, 0.0f);
    tlas_manager_rotate_x(tlas_manager, M_PI / 6.0f);
    tlas_manager_rotate_y(tlas_manager, M_PI / 4.0f);
    tlas_manager_scale(tlas_manager, 0.6f, 0.6f, 0.6f);
    tlas_manager_draw(tlas_manager, cube_blas, 3);
    
    // Large sphere
    tlas_manager_load_identity(tlas_manager);
    tlas_manager_translate(tlas_manager, 0.0f, 3.0f, -2.0f);
    tlas_manager_scale(tlas_manager, 1.5f, 1.5f, 1.5f);
    tlas_manager_draw(tlas_manager, sphere_blas, 1);
    
    // Multiple smaller spheres
    for (int i = 0; i < 3; i++) {
        tlas_manager_load_identity(tlas_manager);
        float x = -2.0f + i * 2.0f;
        tlas_manager_translate(tlas_manager, x, 1.0f, 2.0f);
        tlas_manager_scale(tlas_manager, 0.8f, 0.8f, 0.8f);
        tlas_manager_draw(tlas_manager, sphere_blas, i);
    }
    
    // Hierarchical transforms - use matrix stack
    tlas_manager_load_identity(tlas_manager);
    tlas_manager_translate(tlas_manager, 4.0f, 0.0f, 0.0f);
    
    // Parent transform group
    tlas_manager_push_matrix(tlas_manager);
    {
        tlas_manager_rotate_y(tlas_manager, M_PI / 4.0f);
        
        // Child objects within the group
        for (int i = 0; i < 4; i++) {
            tlas_manager_push_matrix(tlas_manager);
            {
                float angle = (float)i / 4.0f * 2.0f * M_PI;
                tlas_manager_translate(tlas_manager, cosf(angle) * 5.0f, 0.5f, sinf(angle) * 5.0f);
                tlas_manager_scale(tlas_manager, 1.3f, 1.3f, 1.3f);
                tlas_manager_rotate_y(tlas_manager, angle);
                
                // Alternate between cubes and spheres
                BLASHandle geometry = (i % 2 == 0) ? cube_blas : sphere_blas;
                tlas_manager_draw(tlas_manager, geometry, i % 5);
            }
            tlas_manager_pop_matrix(tlas_manager);
        }
    }
    tlas_manager_pop_matrix(tlas_manager);
    
    // Floating objects
    tlas_manager_load_identity(tlas_manager);
    tlas_manager_translate(tlas_manager, -4.0f, 4.0f, -3.0f);
    tlas_manager_rotate_axis(tlas_manager, (Vec3){1.0f, 1.0f, 0.0f}, M_PI / 5.0f);
    tlas_manager_scale(tlas_manager, 0.4f, 0.4f, 0.4f);
    tlas_manager_draw(tlas_manager, cube_blas, 4);
    
    tlas_manager_load_identity(tlas_manager);
    tlas_manager_translate(tlas_manager, 4.0f, 4.0f, -3.0f);
    tlas_manager_scale(tlas_manager, 0.6f, 0.6f, 0.6f);
    tlas_manager_draw(tlas_manager, sphere_blas, 3);
}

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "Modular BLAS/TLAS System Demo");
    SetTargetFPS(60);

    printf("=== Modular BLAS/TLAS System Demo ===\n");
    
    // Create managers
    BLASManager* blas_manager = blas_manager_create();
    TLASManager* tlas_manager = tlas_manager_create(50); // Support up to 50 instances
    
    if (!blas_manager || !tlas_manager) {
        printf("Failed to create managers\n");
        CloseWindow();
        return 1;
    }
    
    // Setup scene using the modular system
    setup_scene(blas_manager, tlas_manager);
    
    // Build TLAS from recorded draw calls
    tlas_manager_build(tlas_manager, blas_manager);
    
    // Print statistics
    blas_manager_print_stats(blas_manager);
    tlas_manager_print_stats(tlas_manager);
    
    // Generate GPU texture data
    int total_triangles  = blas_manager_get_total_triangle_count(blas_manager);
    int total_blas_nodes = blas_manager_get_total_node_count(blas_manager);
    int total_tlas_nodes = tlas_manager_get_node_count(tlas_manager);
    int total_instances  = tlas_manager_get_instance_count(tlas_manager);
    
    printf("GPU Texture Data: %d triangles, %d BLAS nodes, %d TLAS nodes, %d instances\n",
           total_triangles, total_blas_nodes, total_tlas_nodes, total_instances);
    
    // Create combined triangle data
    Triangle* all_triangles = malloc(total_triangles * sizeof(Triangle));
    blas_manager_generate_triangle_texture_data(blas_manager, all_triangles);
    
    // Create combined BLAS node data
    BVHNode* all_blas_nodes = malloc(total_blas_nodes * sizeof(BVHNode));
    blas_manager_generate_node_texture_data(blas_manager, all_blas_nodes);
    
    // Create triangle texture
    int triTextureWidth = total_triangles;
    int triTextureHeight = 4;
    float* triTextureData = (float*)malloc(triTextureWidth * triTextureHeight * 4 * sizeof(float));
    
    for (int i = 0; i < total_triangles; i++) {
        Triangle* tri = &all_triangles[i];
        int baseIdx = i * 4;
        
        // Row 0: v0 + materialId
        triTextureData[baseIdx + 0] = tri->v0.x;
        triTextureData[baseIdx + 1] = tri->v0.y;
        triTextureData[baseIdx + 2] = tri->v0.z;
        triTextureData[baseIdx + 3] = (float)tri->material_id;
        
        // Row 1: v1
        int row1Idx = triTextureWidth * 4 + baseIdx;
        triTextureData[row1Idx + 0] = tri->v1.x;
        triTextureData[row1Idx + 1] = tri->v1.y;
        triTextureData[row1Idx + 2] = tri->v1.z;
        triTextureData[row1Idx + 3] = 0.0f;
        
        // Row 2: v2
        int row2Idx = triTextureWidth * 8 + baseIdx;
        triTextureData[row2Idx + 0] = tri->v2.x;
        triTextureData[row2Idx + 1] = tri->v2.y;
        triTextureData[row2Idx + 2] = tri->v2.z;
        triTextureData[row2Idx + 3] = 0.0f;
        
        // Row 3: normal
        int row3Idx = triTextureWidth * 12 + baseIdx;
        triTextureData[row3Idx + 0] = tri->normal.x;
        triTextureData[row3Idx + 1] = tri->normal.y;
        triTextureData[row3Idx + 2] = tri->normal.z;
        triTextureData[row3Idx + 3] = 0.0f;
    }
    
    Image triImage = {
        .data    = triTextureData,
        .width   = triTextureWidth,
        .height  = triTextureHeight,
        .format  = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,
        .mipmaps = 1
    };
    Texture2D trianglesTexture = LoadTextureFromImage(triImage);
    SetTextureFilter(trianglesTexture, TEXTURE_FILTER_POINT);
    
    // Load raytracing shader
    Shader raytraceShader = LoadShader(NULL, "shaders/raytrace_tlas_blas.fs");
    bool useRaytracing = (raytraceShader.id != 0);
    
    if (raytraceShader.id == 0) {
        printf("Failed to load raytracing shader, using rasterization\n");
    } else {
        printf("Raytracing shader loaded successfully\n");
    }
    
    // Shader uniform locations
    int cameraPosLoc        = GetShaderLocation(raytraceShader, "cameraPos");
    int cameraTargetLoc     = GetShaderLocation(raytraceShader, "cameraTarget");
    int cameraUpLoc         = GetShaderLocation(raytraceShader, "cameraUp");
    int cameraFovyLoc       = GetShaderLocation(raytraceShader, "cameraFovy");
    int screenSizeLoc       = GetShaderLocation(raytraceShader, "screenSize");
    int triangleCountLoc    = GetShaderLocation(raytraceShader, "triangleCount");
    int debugModeLoc        = GetShaderLocation(raytraceShader, "debugMode");
    int trianglesTextureLoc = GetShaderLocation(raytraceShader, "trianglesTexture");
    
    Vector2 screenSize = {(float)screenWidth, (float)screenHeight};
    int debugMode = 5; // Full raytracing

    // Define camera
    Camera camera = { 0 };
    camera.position = (Vector3){ 3.0f, 2.0f, 5.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Main game loop
    while (!WindowShouldClose())
    {
        // Toggle rendering mode
        if (IsKeyPressed(KEY_SPACE)) {
            useRaytracing = !useRaytracing && (raytraceShader.id != 0);
        }
        
        // Debug mode controls
        if (useRaytracing && raytraceShader.id != 0) {
            if (IsKeyPressed(KEY_ONE))   debugMode = 0; // Ray directions
            if (IsKeyPressed(KEY_TWO))   debugMode = 1; // UV coordinates  
            if (IsKeyPressed(KEY_THREE)) debugMode = 2; // TLAS debug
            if (IsKeyPressed(KEY_FOUR))  debugMode = 3; // BLAS debug
            if (IsKeyPressed(KEY_FIVE))  debugMode = 4; // Instance debug
            if (IsKeyPressed(KEY_SIX))   debugMode = 5; // Full raytracing
        }
        
        // Update camera
        UpdateCamera(&camera, CAMERA_FREE);

        // Draw
        BeginDrawing();
            ClearBackground(BLACK);
            
            if (useRaytracing && raytraceShader.id != 0) {
                // Raytracing mode
                BeginShaderMode(raytraceShader);
                
                // Set shader uniforms
                SetShaderValue(raytraceShader, cameraPosLoc, &camera.position, SHADER_UNIFORM_VEC3);
                SetShaderValue(raytraceShader, cameraTargetLoc, &camera.target, SHADER_UNIFORM_VEC3);
                SetShaderValue(raytraceShader, cameraUpLoc, &camera.up, SHADER_UNIFORM_VEC3);
                SetShaderValue(raytraceShader, cameraFovyLoc, &camera.fovy, SHADER_UNIFORM_FLOAT);
                SetShaderValue(raytraceShader, screenSizeLoc, &screenSize, SHADER_UNIFORM_VEC2);
                SetShaderValue(raytraceShader, triangleCountLoc, &total_triangles, SHADER_UNIFORM_INT);
                SetShaderValue(raytraceShader, debugModeLoc, &debugMode, SHADER_UNIFORM_INT);
                
                // Bind textures
                if (trianglesTexture.id != 0 && trianglesTextureLoc != -1) {
                    SetShaderValueTexture(raytraceShader, trianglesTextureLoc, trianglesTexture);
                }
                
                // Draw fullscreen rectangle
                DrawRectangle(0, 0, screenWidth, screenHeight, WHITE);
                
                EndShaderMode();
                
                // UI overlay
                DrawText("MODULAR RAYTRACING", 10, 40, 20, GREEN);
                DrawText("Press SPACE to toggle rasterization", 10, 70, 16, LIGHTGRAY);
                DrawText(TextFormat("Debug Mode %d (Press 1-6)", debugMode), 10, 100, 16, YELLOW);
            } else {
                // Rasterization mode - simplified scene representation
                BeginMode3D(camera);
                    // Draw simplified scene
                    DrawCube((Vector3){0.0f, 0.0f, 0.0f}, 1.0f, 1.0f, 1.0f, RED);
                    DrawCube((Vector3){-3.0f, 0.0f, 0.0f}, 1.0f, 1.0f, 1.0f, BLUE);
                    DrawCube((Vector3){3.0f, 2.0f, 0.0f}, 0.6f, 0.6f, 0.6f, YELLOW);
                    DrawSphere((Vector3){0.0f, 3.0f, -2.0f}, 0.75f, GREEN);
                    DrawPlane((Vector3){0.0f, -1.0f, 0.0f}, (Vector2){20.0f, 20.0f}, DARKGREEN);
                    DrawGrid(10, 1.0f);
                EndMode3D();
                
                DrawText("RASTERIZATION MODE", 10, 40, 20, YELLOW);
                if (raytraceShader.id != 0) {
                    DrawText("Press SPACE to toggle raytracing", 10, 70, 16, LIGHTGRAY);
                } else {
                    DrawText("Raytracing shader failed to load", 10, 70, 16, RED);
                }
            }
            
            DrawFPS(10, 10);
            DrawText("Modular BLAS/TLAS System Demo", 10, screenHeight - 30, 16, LIGHTGRAY);
            
        EndDrawing();
    }

    // Cleanup
    if (raytraceShader.id != 0) UnloadShader(raytraceShader);
    if (trianglesTexture.id != 0) UnloadTexture(trianglesTexture);
    
    free(triTextureData);
    free(all_triangles);
    free(all_blas_nodes);
    
    tlas_manager_destroy(tlas_manager);
    blas_manager_destroy(blas_manager);
    
    CloseWindow();
    return 0;
}