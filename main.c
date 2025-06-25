#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/bvh.h"

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

// Create a cube BLAS (12 triangles)
BLAS* create_cube_blas() {
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
    
    BLAS* blas = blas_create(triangles, 12, 4);
    if (blas) {
        blas_build(blas);
    }
    
    return blas;
}

// Create a high-resolution sphere BLAS
BLAS* create_sphere_blas(float radius, int segments, int rings, int material_id) {
    // Calculate number of triangles: 2 * segments * rings
    int triangle_count = 2 * segments * rings;
    Triangle* triangles = malloc(triangle_count * sizeof(Triangle));
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
            if (ring < rings - 1) { // Don't create triangles for the last ring
                // First triangle
                triangles[tri_idx++] = create_triangle(v1, v2, v3, material_id);
                // Second triangle
                triangles[tri_idx++] = create_triangle(v2, v4, v3, material_id);
            }
        }
    }
    
    // Update triangle count to actual number created
    triangle_count = tri_idx;
    
    printf("Generated sphere with %d triangles (%d segments, %d rings)\n", triangle_count, segments, rings);
    
    BLAS* blas = blas_create(triangles, triangle_count, 4); // Use 4 triangles per leaf for spheres
    if (blas) {
        blas_build(blas);
    }
    
    return blas;
}

// Create a ground plane BLAS (2 triangles)
BLAS* create_ground_blas() {
    Triangle* triangles = malloc(2 * sizeof(Triangle));
    if (!triangles) return NULL;
    
    // Large ground plane
    float size = 10.0f;
    triangles[0] = create_triangle(
        (Vector3){-size, -1.0f, -size}, 
        (Vector3){size, -1.0f, -size}, 
        (Vector3){size, -1.0f, size}, 2);
    triangles[1] = create_triangle(
        (Vector3){-size, -1.0f, -size}, 
        (Vector3){size, -1.0f, size}, 
        (Vector3){-size, -1.0f, size}, 2);
    
    BLAS* blas = blas_create(triangles, 2, 2);
    if (blas) {
        blas_build(blas);
    }
    
    return blas;
}

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "GPU Ray Trace Example - TLAS/BLAS");
    SetTargetFPS(60);

    printf("=== Creating Complex TLAS/BLAS Scene ===\n");
    
    // Create different BLAS geometries
    BLAS* cube_blas = create_cube_blas();
    BLAS* sphere_blas = create_sphere_blas(0.5f, 32, 16, 1); // High-res sphere: 32 segments, 16 rings
    BLAS* ground_blas = create_ground_blas();
    
    if (!cube_blas || !sphere_blas || !ground_blas) {
        printf("Failed to create BLAS geometries\n");
        CloseWindow();
        return 1;
    }
    
    printf("Created BLAS: cube (%d nodes), sphere (%d nodes), ground (%d nodes)\n", 
           cube_blas->node_count, sphere_blas->node_count, ground_blas->node_count);

    // Create TLAS for instancing - support more instances
    TLAS* tlas = tlas_create(20);
    if (!tlas) {
        printf("Failed to create TLAS\n");
        CloseWindow();
        return 1;
    }

    // Create a complex scene with multiple instances
    BVHInstance* instances[18]; // Increased number of instances
    int instance_count = 0;
    
    // === CUBES ===
    // Large red cube at origin
    instances[instance_count] = bvh_instance_create(cube_blas, 0);
    Matrix4x4 trans1          = matrix_translation(0.0f, 0.0f, 0.0f);
    Matrix4x4 scale1          = matrix_scale(1.0f, 1.0f, 1.0f);
    Matrix4x4 transform1      = matrix_multiply(&trans1, &scale1);
    bvh_instance_set_transform(instances[instance_count], &transform1);
    instance_count++;
    
    // Blue cube with rotation
    instances[instance_count] = bvh_instance_create(cube_blas, 1);
    Matrix4x4 trans2 = matrix_translation(-3.0f, 0.0f, 0.0f);
    Matrix4x4 rot2 = matrix_rotation_y(M_PI / 3.0f);
    Matrix4x4 scale2 = matrix_scale(0.8f, 1.2f, 0.8f);
    Matrix4x4 temp2 = matrix_multiply(&rot2, &scale2);
    Matrix4x4 transform2 = matrix_multiply(&trans2, &temp2);
    bvh_instance_set_transform(instances[instance_count], &transform2);
    instance_count++;
    
    // Yellow cube elevated and tilted
    instances[instance_count] = bvh_instance_create(cube_blas, 3);
    Matrix4x4 trans3 = matrix_translation(3.0f, 2.0f, 0.0f);
    Matrix4x4 rotX3 = matrix_rotation_x(M_PI / 6.0f);
    Matrix4x4 rotY3 = matrix_rotation_y(M_PI / 4.0f);
    Matrix4x4 scale3 = matrix_scale(0.6f, 0.6f, 0.6f);
    Matrix4x4 temp3a = matrix_multiply(&rotX3, &rotY3);
    Matrix4x4 temp3b = matrix_multiply(&temp3a, &scale3);
    Matrix4x4 transform3 = matrix_multiply(&trans3, &temp3b);
    bvh_instance_set_transform(instances[instance_count], &transform3);
    instance_count++;
    
    // === SPHERES ===
    // Large sphere at elevated position
    instances[instance_count] = bvh_instance_create(sphere_blas, 1);
    Matrix4x4 trans4 = matrix_translation(0.0f, 3.0f, -2.0f);
    Matrix4x4 scale4 = matrix_scale(1.5f, 1.5f, 1.5f);
    Matrix4x4 transform4 = matrix_multiply(&trans4, &scale4);
    bvh_instance_set_transform(instances[instance_count], &transform4);
    instance_count++;
    
    // Red sphere
    instances[instance_count] = bvh_instance_create(sphere_blas, 0);
    Matrix4x4 trans5 = matrix_translation(-2.0f, 1.0f, 2.0f);
    Matrix4x4 scale5 = matrix_scale(0.8f, 0.8f, 0.8f);
    Matrix4x4 transform5 = matrix_multiply(&trans5, &scale5);
    bvh_instance_set_transform(instances[instance_count], &transform5);
    instance_count++;
    
    // Green sphere
    instances[instance_count] = bvh_instance_create(sphere_blas, 2);
    Matrix4x4 trans6 = matrix_translation(2.0f, 1.0f, 2.0f);
    Matrix4x4 scale6 = matrix_scale(1.0f, 1.0f, 1.0f);
    Matrix4x4 transform6 = matrix_multiply(&trans6, &scale6);
    bvh_instance_set_transform(instances[instance_count], &transform6);
    instance_count++;
    
    // Magenta sphere with anisotropic scaling
    instances[instance_count] = bvh_instance_create(sphere_blas, 4);
    Matrix4x4 trans7 = matrix_translation(-1.0f, 2.5f, -1.0f);
    Matrix4x4 scale7 = matrix_scale(0.5f, 1.5f, 0.5f); // Stretched vertically
    Matrix4x4 transform7 = matrix_multiply(&trans7, &scale7);
    bvh_instance_set_transform(instances[instance_count], &transform7);
    instance_count++;
    
    // === SMALL OBJECTS CLUSTER ===
    // Create a cluster of small cubes and spheres
    for (int i = 0; i < 8; i++) {
        float angle = (float)i / 8.0f * 2.0f * M_PI;
        float x = 4.0f * cosf(angle);
        float z = 4.0f * sinf(angle);
        float y = 0.5f + sinf(angle * 3.0f) * 0.3f; // Varying heights
        
        // Alternate between cubes and spheres
        BLAS* geometry = (i % 2 == 0) ? cube_blas : sphere_blas;
        int material = i % 5; // Cycle through materials
        
        instances[instance_count] = bvh_instance_create(geometry, material);
        Matrix4x4 trans = matrix_translation(x, y, z);
        Matrix4x4 rot = matrix_rotation_y(angle);
        Matrix4x4 scale = matrix_scale(0.3f, 0.3f, 0.3f);
        Matrix4x4 temp_transform = matrix_multiply(&rot, &scale);
        Matrix4x4 final_transform = matrix_multiply(&trans, &temp_transform);
        bvh_instance_set_transform(instances[instance_count], &final_transform);
        instance_count++;
    }
    
    // === FLOATING OBJECTS ===
    // Floating cube
    instances[instance_count] = bvh_instance_create(cube_blas, 4);
    Matrix4x4 trans_float1 = matrix_translation(-4.0f, 4.0f, -3.0f);
    Matrix4x4 rot_float1 = matrix_rotation_axis((Vec3){1.0f, 1.0f, 0.0f}, M_PI / 5.0f);
    Matrix4x4 scale_float1 = matrix_scale(0.4f, 0.4f, 0.4f);
    Matrix4x4 temp_float1 = matrix_multiply(&rot_float1, &scale_float1);
    Matrix4x4 transform_float1 = matrix_multiply(&trans_float1, &temp_float1);
    bvh_instance_set_transform(instances[instance_count], &transform_float1);
    instance_count++;
    
    // Floating sphere
    instances[instance_count] = bvh_instance_create(sphere_blas, 3);
    Matrix4x4 trans_float2 = matrix_translation(4.0f, 4.0f, -3.0f);
    Matrix4x4 scale_float2 = matrix_scale(0.6f, 0.6f, 0.6f);
    Matrix4x4 transform_float2 = matrix_multiply(&trans_float2, &scale_float2);
    bvh_instance_set_transform(instances[instance_count], &transform_float2);
    instance_count++;
    
    // Ground plane instance (always last)
    instances[instance_count] = bvh_instance_create(ground_blas, 2);
    Matrix4x4 ground_transform = matrix_identity();
    bvh_instance_set_transform(instances[instance_count], &ground_transform);
    instance_count++;
    
    printf("Created %d instances total\n", instance_count);
    
    // Add all instances to TLAS
    for (int i = 0; i < instance_count; i++) {
        tlas_add_instance(tlas, instances[i]);
    }
    
    // Build TLAS
    tlas_build(tlas);
    
    printf("TLAS built with %d instances, %d nodes\n", tlas->instance_count, tlas->node_count);
    
    // Prepare GPU data
    int total_triangles  = cube_blas->triangle_count + sphere_blas->triangle_count + ground_blas->triangle_count;
    int total_blas_nodes = cube_blas->node_count     + sphere_blas->node_count     + ground_blas->node_count;
    
    printf("Total GPU data: %d triangles, %d BLAS nodes, %d TLAS nodes\n", 
           total_triangles, total_blas_nodes, tlas->node_count);
    
    // Create GPU textures for TLAS/BLAS data
    Texture2D trianglesTexture = {0};
    Texture2D blasNodesTexture = {0};
    Texture2D tlasNodesTexture = {0};
    Texture2D instancesTexture = {0};
    
    // Prepare all triangle data (combine from all BLAS)
    Triangle* all_triangles = malloc(total_triangles * sizeof(Triangle));
    int tri_offset = 0;
    
    // Copy cube triangles
    memcpy(&all_triangles[tri_offset], cube_blas->triangles, cube_blas->triangle_count * sizeof(Triangle));
    tri_offset += cube_blas->triangle_count;
    
    // Copy sphere triangles
    memcpy(&all_triangles[tri_offset], sphere_blas->triangles, sphere_blas->triangle_count * sizeof(Triangle));
    tri_offset += sphere_blas->triangle_count;
    
    // Copy ground triangles  
    memcpy(&all_triangles[tri_offset], ground_blas->triangles, ground_blas->triangle_count * sizeof(Triangle));
    
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
        .data = triTextureData,
        .width = triTextureWidth,
        .height = triTextureHeight,
        .format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,
        .mipmaps = 1
    };
    trianglesTexture = LoadTextureFromImage(triImage);
    SetTextureFilter(trianglesTexture, TEXTURE_FILTER_POINT);
    
    // Create combined BLAS nodes texture and update instance BLAS start indices
    BVHNode* all_blas_nodes = malloc(total_blas_nodes * sizeof(BVHNode));
    int node_offset = 0;
    int triangle_offset = 0;
    
    // Copy cube BLAS nodes (triangle indices start at 0)
    int cube_start_index = node_offset;
    for (int i = 0; i < cube_blas->node_count; i++) {
        all_blas_nodes[node_offset + i] = cube_blas->nodes[i];
        // Triangle indices remain the same for cube (starts at 0)
    }
    node_offset += cube_blas->node_count;
    triangle_offset += cube_blas->triangle_count;
    
    // Copy sphere BLAS nodes (adjust indices)
    int sphere_start_index = node_offset;
    for (int i = 0; i < sphere_blas->node_count; i++) {
        all_blas_nodes[node_offset + i] = sphere_blas->nodes[i];
        if (all_blas_nodes[node_offset + i].tri_count > 0) { 
            // Leaf node - adjust triangle indices
            all_blas_nodes[node_offset + i].left_first += triangle_offset;
        } else {
            // Internal node - adjust child node indices
            all_blas_nodes[node_offset + i].left_first += sphere_start_index;
        }
    }
    node_offset += sphere_blas->node_count;
    triangle_offset += sphere_blas->triangle_count;
    
    // Copy ground BLAS nodes (adjust indices)
    int ground_start_index = node_offset;
    for (int i = 0; i < ground_blas->node_count; i++) {
        all_blas_nodes[node_offset + i] = ground_blas->nodes[i];
        if (all_blas_nodes[node_offset + i].tri_count > 0) { 
            // Leaf node - adjust triangle indices
            all_blas_nodes[node_offset + i].left_first += triangle_offset;
        } else {
            // Internal node - adjust child node indices
            all_blas_nodes[node_offset + i].left_first += ground_start_index;
        }
    }
    
    // Now update all instances with their correct BLAS start indices
    for (int i = 0; i < tlas->instance_count; i++) {
        BVHInstance* inst = &tlas->instances[i];
        if (inst->blas == cube_blas) {
            inst->blas_start_index = cube_start_index;
        } else if (inst->blas == sphere_blas) {
            inst->blas_start_index = sphere_start_index;
        } else if (inst->blas == ground_blas) {
            inst->blas_start_index = ground_start_index;
        }
    }
    
    printf("BLAS start indices: cube=%d, sphere=%d, ground=%d\n", 
           cube_start_index, sphere_start_index, ground_start_index);
    
    // Create BLAS nodes texture
    int blasNodeTextureWidth = total_blas_nodes;
    int blasNodeTextureHeight = 3;
    float* blasNodeTextureData = (float*)malloc(blasNodeTextureWidth * blasNodeTextureHeight * 4 * sizeof(float));
    
    for (int i = 0; i < total_blas_nodes; i++) {
        BVHNode* node = &all_blas_nodes[i];
        int baseIdx = i * 4;
        
        // Row 0: aabbMin + leftFirst
        blasNodeTextureData[baseIdx + 0] = node->aabb_min.x;
        blasNodeTextureData[baseIdx + 1] = node->aabb_min.y;
        blasNodeTextureData[baseIdx + 2] = node->aabb_min.z;
        blasNodeTextureData[baseIdx + 3] = (float)node->left_first;
        
        // Row 1: aabbMax + triCount
        int row1Idx = blasNodeTextureWidth * 4 + baseIdx;
        blasNodeTextureData[row1Idx + 0] = node->aabb_max.x;
        blasNodeTextureData[row1Idx + 1] = node->aabb_max.y;
        blasNodeTextureData[row1Idx + 2] = node->aabb_max.z;
        blasNodeTextureData[row1Idx + 3] = (float)node->tri_count;
        
        // Row 2: padding
        int row2Idx = blasNodeTextureWidth * 8 + baseIdx;
        blasNodeTextureData[row2Idx + 0] = 0.0f;
        blasNodeTextureData[row2Idx + 1] = 0.0f;
        blasNodeTextureData[row2Idx + 2] = 0.0f;
        blasNodeTextureData[row2Idx + 3] = 0.0f;
    }
    
    Image blasNodeImage = {
        .data = blasNodeTextureData,
        .width = blasNodeTextureWidth,
        .height = blasNodeTextureHeight,
        .format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,
        .mipmaps = 1
    };
    blasNodesTexture = LoadTextureFromImage(blasNodeImage);
    SetTextureFilter(blasNodesTexture, TEXTURE_FILTER_POINT);
    
    // Create TLAS nodes texture
    int tlasNodeTextureWidth = tlas->node_count;
    int tlasNodeTextureHeight = 3;
    float* tlasNodeTextureData = (float*)malloc(tlasNodeTextureWidth * tlasNodeTextureHeight * 4 * sizeof(float));
    
    for (int i = 0; i < tlas->node_count; i++) {
        TLASNode* node = &tlas->nodes[i];
        int baseIdx = i * 4;
        
        // Row 0: aabbMin + leftRight
        tlasNodeTextureData[baseIdx + 0] = node->aabb_min.x;
        tlasNodeTextureData[baseIdx + 1] = node->aabb_min.y;
        tlasNodeTextureData[baseIdx + 2] = node->aabb_min.z;
        tlasNodeTextureData[baseIdx + 3] = (float)node->left_right;
        
        // Row 1: aabbMax + blasIndex
        int row1Idx = tlasNodeTextureWidth * 4 + baseIdx;
        tlasNodeTextureData[row1Idx + 0] = node->aabb_max.x;
        tlasNodeTextureData[row1Idx + 1] = node->aabb_max.y;
        tlasNodeTextureData[row1Idx + 2] = node->aabb_max.z;
        tlasNodeTextureData[row1Idx + 3] = (float)node->blas_index;
        
        // Row 2: padding
        int row2Idx = tlasNodeTextureWidth * 8 + baseIdx;
        tlasNodeTextureData[row2Idx + 0] = 0.0f;
        tlasNodeTextureData[row2Idx + 1] = 0.0f;
        tlasNodeTextureData[row2Idx + 2] = 0.0f;
        tlasNodeTextureData[row2Idx + 3] = 0.0f;
    }
    
    Image tlasNodeImage = {
        .data = tlasNodeTextureData,
        .width = tlasNodeTextureWidth,
        .height = tlasNodeTextureHeight,
        .format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,
        .mipmaps = 1
    };
    tlasNodesTexture = LoadTextureFromImage(tlasNodeImage);
    SetTextureFilter(tlasNodesTexture, TEXTURE_FILTER_POINT);
    
    // Create instances texture  
    int instancesTextureWidth = tlas->instance_count;
    int instancesTextureHeight = 9; // Need 9 rows: two 4x4 matrices + metadata row
    float* instancesTextureData = (float*)malloc(instancesTextureWidth * instancesTextureHeight * 4 * sizeof(float));
    
    for (int i = 0; i < tlas->instance_count; i++) {
        BVHInstance* inst = &tlas->instances[i];
        int baseIdx = i * 4;
        
        // Rows 0-3: transform matrix (4x4)
        for (int row = 0; row < 4; row++) {
            int rowIdx = instancesTextureWidth * (row * 4) + baseIdx;
            instancesTextureData[rowIdx + 0] = inst->transform.m[row * 4 + 0];
            instancesTextureData[rowIdx + 1] = inst->transform.m[row * 4 + 1];
            instancesTextureData[rowIdx + 2] = inst->transform.m[row * 4 + 2];
            instancesTextureData[rowIdx + 3] = inst->transform.m[row * 4 + 3];
        }
        
        // Rows 4-7: inverse transform matrix (4x4)
        for (int row = 0; row < 4; row++) {
            int rowIdx = instancesTextureWidth * ((row + 4) * 4) + baseIdx;
            instancesTextureData[rowIdx + 0] = inst->inv_transform.m[row * 4 + 0];
            instancesTextureData[rowIdx + 1] = inst->inv_transform.m[row * 4 + 1];
            instancesTextureData[rowIdx + 2] = inst->inv_transform.m[row * 4 + 2];
            instancesTextureData[rowIdx + 3] = inst->inv_transform.m[row * 4 + 3];
        }
        
        // Row 8: metadata (blasStartIndex + instanceId + padding)
        int metadataIdx = instancesTextureWidth * (8 * 4) + baseIdx;
        instancesTextureData[metadataIdx + 0] = (float)inst->blas_start_index;
        instancesTextureData[metadataIdx + 1] = (float)inst->instance_id;
        instancesTextureData[metadataIdx + 2] = 0.0f; // padding
        instancesTextureData[metadataIdx + 3] = 0.0f; // padding
    }
    
    Image instancesImage = {
        .data    = instancesTextureData,
        .width   = instancesTextureWidth,
        .height  = instancesTextureHeight,
        .format  = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,
        .mipmaps = 1
    };
    instancesTexture = LoadTextureFromImage(instancesImage);
    SetTextureFilter(instancesTexture, TEXTURE_FILTER_POINT);
    
    // Clean up temporary data
    free(triTextureData);
    free(blasNodeTextureData);
    free(tlasNodeTextureData);
    free(instancesTextureData);
    free(all_triangles);
    free(all_blas_nodes);
    
    printf("TLAS/BLAS textures created: triangles %dx%d, BLAS nodes %dx%d, TLAS nodes %dx%d, instances %dx%d\n", 
           triTextureWidth, triTextureHeight, blasNodeTextureWidth, blasNodeTextureHeight, 
           tlasNodeTextureWidth, tlasNodeTextureHeight, instancesTextureWidth, instancesTextureHeight);

    // Load TLAS/BLAS raytracing shader
    Shader raytraceShader = LoadShader(NULL, "shaders/raytrace_tlas_blas.fs");
    
    if (raytraceShader.id == 0) {
        printf("Failed to load unified raytracing shader, falling back to rasterization\n");
    } else {
        printf("Unified raytracing shader loaded successfully (ID: %d)\n", raytraceShader.id);
    }

    // Get shader uniform locations
    int cameraPosLoc        = GetShaderLocation(raytraceShader, "cameraPos");
    int cameraTargetLoc     = GetShaderLocation(raytraceShader, "cameraTarget");
    int cameraUpLoc         = GetShaderLocation(raytraceShader, "cameraUp");
    int cameraFovyLoc       = GetShaderLocation(raytraceShader, "cameraFovy");
    int screenSizeLoc       = GetShaderLocation(raytraceShader, "screenSize");
    int triangleCountLoc    = GetShaderLocation(raytraceShader, "triangleCount");
    int blasNodeCountLoc    = GetShaderLocation(raytraceShader, "blasNodeCount");
    int tlasNodeCountLoc    = GetShaderLocation(raytraceShader, "tlasNodeCount");
    int instanceCountLoc    = GetShaderLocation(raytraceShader, "instanceCount");
    int debugModeLoc        = GetShaderLocation(raytraceShader, "debugMode");
    int intersectionModeLoc = GetShaderLocation(raytraceShader, "intersectionMode");
    
    // TLAS/BLAS texture locations
    int trianglesTextureLoc = GetShaderLocation(raytraceShader, "trianglesTexture");
    int blasNodesTextureLoc = GetShaderLocation(raytraceShader, "blasNodesTexture");
    int tlasNodesTextureLoc = GetShaderLocation(raytraceShader, "tlasNodesTexture");
    int instancesTextureLoc = GetShaderLocation(raytraceShader, "instancesTexture");

    Vector2 screenSize = {(float)screenWidth, (float)screenHeight};
    
    // Debug uniform locations
    printf("Uniform locations: debugMode=%d, screenSize=%d, cameraPos=%d\n", 
           debugModeLoc, screenSizeLoc, cameraPosLoc);

    // Debug and rendering state
    int debugMode        = 5; // 0=ray dirs, 1=uv coords, 2=TLAS debug, 3=BLAS debug, 4=instance debug, 5=full raytracing
    int intersectionMode = 1; // 0=brute force, 1=TLAS/BLAS (default to TLAS/BLAS for performance)

    // Define camera
    Camera camera     = { 0 };
    camera.position   = (Vector3){ 3.0f, 2.0f, 5.0f };
    camera.target     = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up         = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy       = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // No render texture needed for simple approach

    bool useRaytracing = (raytraceShader.id != 0);
    
    // Main game loop
    while (!WindowShouldClose())
    {
        // Toggle between raytracing and rasterization
        if (IsKeyPressed(KEY_SPACE)) {
            useRaytracing = !useRaytracing && (raytraceShader.id != 0);
        }
        
        // Debug mode controls (only when raytracing is active)
        if (useRaytracing && raytraceShader.id != 0) {
            if (IsKeyPressed(KEY_ONE))   debugMode = 0; // Ray directions
            if (IsKeyPressed(KEY_TWO))   debugMode = 1; // UV coordinates
            if (IsKeyPressed(KEY_THREE)) debugMode = 2; // TLAS debug
            if (IsKeyPressed(KEY_FOUR))  debugMode = 3; // BLAS vs TLAS comparison
            if (IsKeyPressed(KEY_FIVE))  debugMode = 4; // Instance ID visualization
            if (IsKeyPressed(KEY_SIX))   debugMode = 5; // Full raytracing
            
            // Intersection mode controls
            if (IsKeyPressed(KEY_B)) {
                intersectionMode = (intersectionMode == 0) ? 1 : 0;
                printf("Intersection mode: %s\n", intersectionMode == 0 ? "Brute Force" : "TLAS/BLAS");
            }
        }
        
        // Update camera
        UpdateCamera(&camera, CAMERA_FREE);

        // Draw
        BeginDrawing();
            ClearBackground(BLACK);
            
            if (useRaytracing && raytraceShader.id != 0) {
                // Raytracing mode - use shader for fullscreen quad
                BeginShaderMode(raytraceShader);
                
                // Set shader uniforms
                SetShaderValue(raytraceShader, cameraPosLoc,        &camera.position,      SHADER_UNIFORM_VEC3);
                SetShaderValue(raytraceShader, cameraTargetLoc,     &camera.target,        SHADER_UNIFORM_VEC3);
                SetShaderValue(raytraceShader, cameraUpLoc,         &camera.up,            SHADER_UNIFORM_VEC3);
                SetShaderValue(raytraceShader, cameraFovyLoc,       &camera.fovy,          SHADER_UNIFORM_FLOAT);
                
                SetShaderValue(raytraceShader, screenSizeLoc,       &screenSize,           SHADER_UNIFORM_VEC2);
                SetShaderValue(raytraceShader, triangleCountLoc,    &total_triangles,      SHADER_UNIFORM_INT);
                SetShaderValue(raytraceShader, blasNodeCountLoc,    &total_blas_nodes,     SHADER_UNIFORM_INT);
                SetShaderValue(raytraceShader, tlasNodeCountLoc,    &tlas->node_count,     SHADER_UNIFORM_INT);
                SetShaderValue(raytraceShader, instanceCountLoc,    &tlas->instance_count, SHADER_UNIFORM_INT);
                SetShaderValue(raytraceShader, debugModeLoc,        &debugMode,            SHADER_UNIFORM_INT);
                SetShaderValue(raytraceShader, intersectionModeLoc, &intersectionMode,     SHADER_UNIFORM_INT);
                
                // Bind TLAS/BLAS textures
                if (trianglesTexture.id != 0 && trianglesTextureLoc != -1) {
                    SetShaderValueTexture(raytraceShader, trianglesTextureLoc, trianglesTexture);
                }
                
                if (blasNodesTexture.id != 0 && blasNodesTextureLoc != -1) {
                    SetShaderValueTexture(raytraceShader, blasNodesTextureLoc, blasNodesTexture);
                }
                
                if (tlasNodesTexture.id != 0 && tlasNodesTextureLoc != -1) {
                    SetShaderValueTexture(raytraceShader, tlasNodesTextureLoc, tlasNodesTexture);
                }
                
                if (instancesTexture.id != 0 && instancesTextureLoc != -1) {
                    SetShaderValueTexture(raytraceShader, instancesTextureLoc, instancesTexture);
                }
                
                // Draw fullscreen rectangle
                DrawRectangle(0, 0, screenWidth, screenHeight, WHITE);
                
                EndShaderMode();
                
                // UI overlay
                DrawText("RAYTRACING MODE", 10, 40, 20, GREEN);
                DrawText("Press SPACE to toggle rasterization", 10, 70, 16, LIGHTGRAY);
                
                // Debug mode info
                const char* debugModeNames[] = {
                    "Ray Directions", "UV Coordinates", "TLAS Debug", 
                    "BLAS vs TLAS Comparison", "Instance ID Visualization", "Full Raytracing"
                };
                int maxMode = sizeof(debugModeNames) / sizeof(debugModeNames[0]) - 1;
                if (debugMode <= maxMode) {
                    DrawText(TextFormat("Debug Mode %d: %s", debugMode, debugModeNames[debugMode]), 
                             10, 100, 16, YELLOW);
                }
                DrawText("Press 1-6 to change debug mode", 10, 120, 14, LIGHTGRAY);
                
                // Show intersection mode
                const char* intersectionModeText = intersectionMode == 0 ? "Brute Force" : "TLAS/BLAS";
                DrawText(TextFormat("Intersection: %s (Press B to toggle)", intersectionModeText), 
                         10, 140, 14, SKYBLUE);
            } else {
                // Rasterization mode
                BeginMode3D(camera);
                    // Draw scene using traditional rasterization
                    DrawCube((Vector3){-2.0f, 0.0f, 0.0f}, 1.0f, 1.0f, 1.0f, RED);
                    DrawCube((Vector3){2.0f, 0.0f, 0.0f}, 1.0f, 1.0f, 1.0f, BLUE);
                    DrawPlane((Vector3){0.0f, -2.0f, 0.0f}, (Vector2){4.0f, 4.0f}, GREEN);
                    DrawSphere((Vector3){0.0f, 1.0f, 1.0f}, 0.5f, YELLOW);
                    DrawSphere((Vector3){0.0f, 1.0f, -1.0f}, 0.3f, MAGENTA);
                    DrawGrid(10, 1.0f);
                EndMode3D();
                
                // UI overlay
                DrawText("RASTERIZATION MODE", 10, 40, 20, YELLOW);
                if (raytraceShader.id != 0) {
                    DrawText("Press SPACE to toggle raytracing", 10, 70, 16, LIGHTGRAY);
                } else {
                    DrawText("Raytracing shader failed to load", 10, 70, 16, RED);
                }
            }
            
            DrawFPS(10, 10);
            DrawText("Use WASD to move, mouse to look around", 10, screenHeight - 30, 16, LIGHTGRAY);
            
        EndDrawing();
    }

    // Cleanup
    if (raytraceShader.id != 0) UnloadShader(raytraceShader);
    if (trianglesTexture.id != 0) UnloadTexture(trianglesTexture);
    if (blasNodesTexture.id != 0) UnloadTexture(blasNodesTexture);
    if (tlasNodesTexture.id != 0) UnloadTexture(tlasNodesTexture);
    if (instancesTexture.id != 0) UnloadTexture(instancesTexture);
    
    // Cleanup instances
    for (int i = 0; i < instance_count; i++) {
        bvh_instance_destroy(instances[i]);
    }
    
    // Cleanup TLAS and BLAS
    tlas_destroy(tlas);
    blas_destroy(cube_blas);
    blas_destroy(sphere_blas);
    blas_destroy(ground_blas);
    
    CloseWindow();
    return 0;
}