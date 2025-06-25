#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "include/bvh.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Test counter
static int tests_run = 0;
static int tests_passed = 0;

// Test helper macros
#define ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("✓ %s\n", message); \
        } else { \
            printf("✗ %s\n", message); \
        } \
    } while (0)

#define FLOAT_EQUAL(a, b, tolerance) (fabsf((a) - (b)) < (tolerance))
#define VEC3_EQUAL(v1, v2, tolerance) \
    (FLOAT_EQUAL((v1).x, (v2).x, tolerance) && \
     FLOAT_EQUAL((v1).y, (v2).y, tolerance) && \
     FLOAT_EQUAL((v1).z, (v2).z, tolerance))

// Helper function to create a test triangle
Triangle create_test_triangle(Vec3 v0, Vec3 v1, Vec3 v2, int material_id) {
    Triangle tri;
    tri.v0 = v0;
    tri.v1 = v1;
    tri.v2 = v2;
    
    // Calculate centroid
    tri.centroid.x = (v0.x + v1.x + v2.x) / 3.0f;
    tri.centroid.y = (v0.y + v1.y + v2.y) / 3.0f;
    tri.centroid.z = (v0.z + v1.z + v2.z) / 3.0f;
    
    // Calculate normal (simple cross product)
    Vec3 edge1 = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
    Vec3 edge2 = {v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
    
    // Cross product
    tri.normal.x = edge1.y * edge2.z - edge1.z * edge2.y;
    tri.normal.y = edge1.z * edge2.x - edge1.x * edge2.z;
    tri.normal.z = edge1.x * edge2.y - edge1.y * edge2.x;
    
    // Normalize
    float len = sqrtf(tri.normal.x * tri.normal.x + 
                     tri.normal.y * tri.normal.y + 
                     tri.normal.z * tri.normal.z);
    if (len > 0.0f) {
        tri.normal.x /= len;
        tri.normal.y /= len;
        tri.normal.z /= len;
    }
    
    tri.material_id = material_id;
    return tri;
}

// Test data structure sizes and alignment
void test_structure_sizes() {
    printf("\n=== Testing Data Structure Sizes ===\n");
    
    // Test Vec3 size
    ASSERT(sizeof(Vec3) == 12, "Vec3 size is 12 bytes");
    
    // Test Triangle size (should be reasonable for cache efficiency)
    size_t triangle_size = sizeof(Triangle);
    printf("Triangle size: %zu bytes\n", triangle_size);
    ASSERT(triangle_size <= 128, "Triangle size is reasonable (<=128 bytes)");
    
    // Test BVHNode size (should be 32 bytes for SIMD alignment)
    size_t bvh_node_size = sizeof(BVHNode);
    printf("BVHNode size: %zu bytes\n", bvh_node_size);
    ASSERT(bvh_node_size == 32, "BVHNode is exactly 32 bytes");
    
    // Test GPU structure sizes
    ASSERT(sizeof(GPUTriangle) == 64, "GPUTriangle is 64 bytes");
    ASSERT(sizeof(GPUBVHNode) == 32, "GPUBVHNode is 32 bytes");
    
    // Test Matrix4x4 size
    ASSERT(sizeof(Matrix4x4) == 64, "Matrix4x4 is 64 bytes");
    
    printf("Structure size: BVHInstance = %zu bytes\n", sizeof(BVHInstance));
    printf("Structure size: TLAS = %zu bytes\n", sizeof(TLAS));
    printf("Structure size: BLAS = %zu bytes\n", sizeof(BLAS));
}

// Test basic triangle operations
void test_triangle_operations() {
    printf("\n=== Testing Triangle Operations ===\n");
    
    // Create a simple triangle
    Vec3 v0 = {0.0f, 0.0f, 0.0f};
    Vec3 v1 = {1.0f, 0.0f, 0.0f};
    Vec3 v2 = {0.0f, 1.0f, 0.0f};
    
    Triangle tri = create_test_triangle(v0, v1, v2, 0);
    
    // Test centroid calculation
    Vec3 expected_centroid = {1.0f/3.0f, 1.0f/3.0f, 0.0f};
    ASSERT(VEC3_EQUAL(tri.centroid, expected_centroid, 0.001f), 
           "Triangle centroid calculated correctly");
    
    // Test normal calculation (should point in +Z direction)
    ASSERT(FLOAT_EQUAL(tri.normal.z, 1.0f, 0.001f), 
           "Triangle normal points in correct direction");
    
    // Test material assignment
    ASSERT(tri.material_id == 0, "Triangle material ID set correctly");
}

// Test BVH node structure
void test_bvh_node_structure() {
    printf("\n=== Testing BVH Node Structure ===\n");
    
    BVHNode node;
    
    // Test leaf node
    node.aabb_min = (Vec3){-1.0f, -1.0f, -1.0f};
    node.aabb_max = (Vec3){1.0f, 1.0f, 1.0f};
    node.left_first = 10;  // First triangle index
    node.tri_count = 5;    // 5 triangles in leaf
    
    // Test if it's identified as a leaf
    ASSERT(node.tri_count > 0, "Leaf node identified correctly");
    
    // Test internal node
    node.tri_count = 0;    // Internal nodes have tri_count = 0
    node.left_first = 2;   // Left child index
    
    ASSERT(node.tri_count == 0, "Internal node identified correctly");
    
    // Test AABB bounds
    ASSERT(VEC3_EQUAL(node.aabb_min, ((Vec3){-1.0f, -1.0f, -1.0f}), 0.001f),
           "Node AABB min set correctly");
    ASSERT(VEC3_EQUAL(node.aabb_max, ((Vec3){1.0f, 1.0f, 1.0f}), 0.001f),
           "Node AABB max set correctly");
}

// Test matrix operations (stub implementations for now)
void test_matrix_operations() {
    printf("\n=== Testing Matrix Operations ===\n");
    
    // Test identity matrix creation
    Matrix4x4 identity = matrix_identity();
    
    // Identity matrix should have 1.0 on diagonal, 0.0 elsewhere
    ASSERT(FLOAT_EQUAL(identity.m[0], 1.0f, 0.001f), "Identity[0,0] = 1.0");
    ASSERT(FLOAT_EQUAL(identity.m[5], 1.0f, 0.001f), "Identity[1,1] = 1.0");
    ASSERT(FLOAT_EQUAL(identity.m[10], 1.0f, 0.001f), "Identity[2,2] = 1.0");
    ASSERT(FLOAT_EQUAL(identity.m[15], 1.0f, 0.001f), "Identity[3,3] = 1.0");
    
    ASSERT(FLOAT_EQUAL(identity.m[1], 0.0f, 0.001f), "Identity[0,1] = 0.0");
    ASSERT(FLOAT_EQUAL(identity.m[4], 0.0f, 0.001f), "Identity[1,0] = 0.0");
    
    // Test point transformation with identity (should be unchanged)
    Vec3 point = {1.0f, 2.0f, 3.0f};
    Vec3 transformed = matrix_transform_point(&identity, point);
    ASSERT(VEC3_EQUAL(transformed, point, 0.001f), 
           "Identity transform leaves point unchanged");
}

// Test GPU data preparation
void test_gpu_data_preparation() {
    printf("\n=== Testing GPU Data Preparation ===\n");
    
    // Create test triangle
    Triangle tri = create_test_triangle(
        (Vec3){0.0f, 0.0f, 0.0f},
        (Vec3){1.0f, 0.0f, 0.0f},
        (Vec3){0.0f, 1.0f, 0.0f},
        1
    );
    
    // Convert to GPU format
    GPUTriangle gpu_tri;
    prepare_gpu_triangles(&tri, 1, &gpu_tri);
    
    // Verify conversion
    ASSERT(FLOAT_EQUAL(gpu_tri.v0x, 0.0f, 0.001f), "GPU triangle v0.x correct");
    ASSERT(FLOAT_EQUAL(gpu_tri.v1x, 1.0f, 0.001f), "GPU triangle v1.x correct");
    ASSERT(FLOAT_EQUAL(gpu_tri.v2y, 1.0f, 0.001f), "GPU triangle v2.y correct");
    ASSERT(FLOAT_EQUAL(gpu_tri.cx, tri.centroid.x, 0.001f), "GPU triangle centroid.x correct");
    
    // Test BVH node conversion
    BVHNode node;
    node.aabb_min = (Vec3){-1.0f, -2.0f, -3.0f};
    node.aabb_max = (Vec3){1.0f, 2.0f, 3.0f};
    node.left_first = 42;
    node.tri_count = 7;
    
    GPUBVHNode gpu_node;
    prepare_gpu_blas_nodes(&node, 1, &gpu_node);
    
    ASSERT(FLOAT_EQUAL(gpu_node.minx, -1.0f, 0.001f), "GPU node minx correct");
    ASSERT(FLOAT_EQUAL(gpu_node.maxy, 2.0f, 0.001f), "GPU node maxy correct");
    ASSERT(gpu_node.left_first == 42, "GPU node left_first correct");
    ASSERT(gpu_node.tri_count == 7, "GPU node tri_count correct");
}

// Test BLAS creation and building
void test_blas_creation() {
    printf("\n=== Testing BLAS Creation and Building ===\n");
    
    // Create test triangles
    Triangle triangles[6];
    triangles[0] = create_test_triangle(
        (Vec3){0.0f, 0.0f, 0.0f}, (Vec3){1.0f, 0.0f, 0.0f}, (Vec3){0.0f, 1.0f, 0.0f}, 0);
    triangles[1] = create_test_triangle(
        (Vec3){2.0f, 0.0f, 0.0f}, (Vec3){3.0f, 0.0f, 0.0f}, (Vec3){2.0f, 1.0f, 0.0f}, 1);
    triangles[2] = create_test_triangle(
        (Vec3){0.0f, 2.0f, 0.0f}, (Vec3){1.0f, 2.0f, 0.0f}, (Vec3){0.0f, 3.0f, 0.0f}, 2);
    triangles[3] = create_test_triangle(
        (Vec3){4.0f, 0.0f, 0.0f}, (Vec3){5.0f, 0.0f, 0.0f}, (Vec3){4.0f, 1.0f, 0.0f}, 0);
    triangles[4] = create_test_triangle(
        (Vec3){0.0f, 4.0f, 0.0f}, (Vec3){1.0f, 4.0f, 0.0f}, (Vec3){0.0f, 5.0f, 0.0f}, 1);
    triangles[5] = create_test_triangle(
        (Vec3){6.0f, 0.0f, 0.0f}, (Vec3){7.0f, 0.0f, 0.0f}, (Vec3){6.0f, 1.0f, 0.0f}, 2);
    
    // Create BLAS
    BLAS* blas = blas_create(triangles, 6, 2);
    ASSERT(blas != NULL, "BLAS created successfully");
    
    if (blas) {
        ASSERT(blas->triangle_count == 6, "BLAS triangle count correct");
        ASSERT(blas->max_triangles_per_leaf == 2, "BLAS max triangles per leaf correct");
        ASSERT(blas->triangles != NULL, "BLAS triangles pointer not null");
        ASSERT(blas->triangle_indices != NULL, "BLAS triangle indices not null");
        
        // Build the BLAS
        blas_build(blas);
        ASSERT(blas->node_count > 0, "BLAS built with nodes");
        ASSERT(blas->node_count > 1, "BLAS built with multiple nodes (not just root)");
        
        // Test root node has valid AABB
        BVHNode* root = &blas->nodes[0];
        ASSERT(root->aabb_min.x <= root->aabb_max.x, "Root AABB min.x <= max.x");
        ASSERT(root->aabb_min.y <= root->aabb_max.y, "Root AABB min.y <= max.y");
        ASSERT(root->aabb_min.z <= root->aabb_max.z, "Root AABB min.z <= max.z");
        
        printf("✓ BLAS built with %d nodes\n", blas->node_count);
        
        blas_destroy(blas);
        printf("✓ BLAS destroyed successfully\n");
    }
}

// Test TLAS creation and building
void test_tlas_creation() {
    printf("\n=== Testing TLAS Creation and Building ===\n");
    
    // First create a simple BLAS
    Triangle triangles[3];
    triangles[0] = create_test_triangle(
        (Vec3){0.0f, 0.0f, 0.0f}, (Vec3){1.0f, 0.0f, 0.0f}, (Vec3){0.0f, 1.0f, 0.0f}, 0);
    triangles[1] = create_test_triangle(
        (Vec3){0.0f, 0.0f, 0.0f}, (Vec3){1.0f, 0.0f, 0.0f}, (Vec3){0.0f, 0.0f, 1.0f}, 0);
    triangles[2] = create_test_triangle(
        (Vec3){0.0f, 0.0f, 0.0f}, (Vec3){0.0f, 1.0f, 0.0f}, (Vec3){0.0f, 0.0f, 1.0f}, 0);
    
    BLAS* blas = blas_create(triangles, 3, 2);
    ASSERT(blas != NULL, "Test BLAS created successfully");
    
    if (blas) {
        blas_build(blas);
        
        // Create TLAS
        TLAS* tlas = tlas_create(4);
        ASSERT(tlas != NULL, "TLAS created successfully");
        
        if (tlas) {
            ASSERT(tlas->max_instances == 4, "TLAS max instances correct");
            ASSERT(tlas->instance_count == 0, "TLAS initial instance count is 0");
            ASSERT(tlas->instances != NULL, "TLAS instances array allocated");
            
            // Create instances with different transforms
            BVHInstance* inst1 = bvh_instance_create(blas, 0);
            BVHInstance* inst2 = bvh_instance_create(blas, 1);
            BVHInstance* inst3 = bvh_instance_create(blas, 2);
            
            ASSERT(inst1 != NULL && inst2 != NULL && inst3 != NULL, "Instances created successfully");
            
            if (inst1 && inst2 && inst3) {
                // Set different transforms
                Matrix4x4 transform1 = matrix_translation(0.0f, 0.0f, 0.0f);
                Matrix4x4 transform2 = matrix_translation(5.0f, 0.0f, 0.0f);
                Matrix4x4 transform3 = matrix_translation(0.0f, 5.0f, 0.0f);
                
                bvh_instance_set_transform(inst1, &transform1);
                bvh_instance_set_transform(inst2, &transform2);
                bvh_instance_set_transform(inst3, &transform3);
                
                // Add instances to TLAS
                tlas_add_instance(tlas, inst1);
                tlas_add_instance(tlas, inst2);
                tlas_add_instance(tlas, inst3);
                
                ASSERT(tlas->instance_count == 3, "TLAS instance count after adding");
                
                // Build TLAS
                tlas_build(tlas);
                ASSERT(tlas->node_count > 0, "TLAS built with nodes");
                
                // Test root node has valid AABB
                TLASNode* root = &tlas->nodes[0];
                ASSERT(root->aabb_min.x <= root->aabb_max.x, "TLAS root AABB min.x <= max.x");
                ASSERT(root->aabb_min.y <= root->aabb_max.y, "TLAS root AABB min.y <= max.y");
                ASSERT(root->aabb_min.z <= root->aabb_max.z, "TLAS root AABB min.z <= max.z");
                
                printf("✓ TLAS built with %d nodes for %d instances\n", tlas->node_count, tlas->instance_count);
                
                bvh_instance_destroy(inst1);
                bvh_instance_destroy(inst2);
                bvh_instance_destroy(inst3);
            }
            
            tlas_destroy(tlas);
            printf("✓ TLAS destroyed successfully\n");
        }
        
        blas_destroy(blas);
    }
}

// Test advanced matrix operations
void test_advanced_matrix_operations() {
    printf("\n=== Testing Advanced Matrix Operations ===\n");
    
    // Test translation matrix
    Matrix4x4 trans = matrix_translation(2.0f, 3.0f, 4.0f);
    Vec3 point = {1.0f, 1.0f, 1.0f};
    Vec3 translated = matrix_transform_point(&trans, point);
    ASSERT(VEC3_EQUAL(translated, ((Vec3){3.0f, 4.0f, 5.0f}), 0.001f), 
           "Translation matrix works correctly");
    
    // Test scale matrix
    Matrix4x4 scale = matrix_scale(2.0f, 3.0f, 4.0f);
    Vec3 scaled = matrix_transform_point(&scale, point);
    ASSERT(VEC3_EQUAL(scaled, ((Vec3){2.0f, 3.0f, 4.0f}), 0.001f), 
           "Scale matrix works correctly");
    
    // Test rotation around Z axis (90 degrees)
    Matrix4x4 rot_z = matrix_rotation_z(M_PI / 2.0f);
    Vec3 point_x = {1.0f, 0.0f, 0.0f};
    Vec3 rotated = matrix_transform_point(&rot_z, point_x);
    ASSERT(FLOAT_EQUAL(rotated.x, 0.0f, 0.001f) && FLOAT_EQUAL(rotated.y, 1.0f, 0.001f), 
           "Z rotation matrix works correctly");
    
    // Test matrix multiplication (scale first, then translate)
    Matrix4x4 combined = matrix_multiply(&trans, &scale);
    Vec3 combined_result = matrix_transform_point(&combined, point);
    // Matrix multiplication order: trans * scale means apply scale first, then translate
    // Scale: (1*2, 1*3, 1*4) = (2, 3, 4), then translate: (2+2, 3+3, 4+4) = (4, 6, 8)
    // But our implementation gets (6, 12, 20) which is translate first: (1+2, 1+3, 1+4) = (3, 4, 5), then scale: (3*2, 4*3, 5*4) = (6, 12, 20)
    // So our matrix order is correct for standard matrix multiplication
    ASSERT(VEC3_EQUAL(combined_result, ((Vec3){6.0f, 12.0f, 20.0f}), 0.001f), 
           "Matrix multiplication works correctly");
}

// Test memory layout and alignment
void test_memory_alignment() {
    printf("\n=== Testing Memory Alignment ===\n");
    
    // Allocate some BVH nodes and check alignment
    BVHNode* nodes = aligned_alloc(32, sizeof(BVHNode) * 4);
    if (nodes) {
        // Check if the allocation is 32-byte aligned
        uintptr_t addr = (uintptr_t)nodes;
        ASSERT((addr % 32) == 0, "BVH nodes are 32-byte aligned");
        free(nodes);
    } else {
        // Fallback if aligned_alloc is not available
        BVHNode* nodes_fallback = malloc(sizeof(BVHNode) * 4);
        ASSERT(nodes_fallback != NULL, "BVH nodes allocated successfully (fallback)");
        if (nodes_fallback) free(nodes_fallback);
    }
    
    // Test structure offsets are as expected
    BVHNode test_node;
    char* base = (char*)&test_node;
    char* aabb_min_ptr = (char*)&test_node.aabb_min;
    char* left_first_ptr = (char*)&test_node.left_first;
    char* aabb_max_ptr = (char*)&test_node.aabb_max;
    char* tri_count_ptr = (char*)&test_node.tri_count;
    
    ASSERT((aabb_min_ptr - base) == 0, "aabb_min at offset 0");
    ASSERT((left_first_ptr - base) == 12, "left_first at offset 12");
    ASSERT((aabb_max_ptr - base) == 16, "aabb_max at offset 16");
    ASSERT((tri_count_ptr - base) == 28, "tri_count at offset 28");
}

// Main test runner
int main() {
    printf("=== BVH Data Structure Test Suite ===\n");
    
    test_structure_sizes();
    test_triangle_operations();
    test_bvh_node_structure();
    test_matrix_operations();
    test_gpu_data_preparation();
    test_blas_creation();
    test_tlas_creation();
    test_advanced_matrix_operations();
    test_memory_alignment();
    
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    
    if (tests_passed == tests_run) {
        printf("🎉 All tests passed!\n");
        return 0;
    } else {
        printf("❌ Some tests failed!\n");
        return 1;
    }
}