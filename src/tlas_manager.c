#include "../include/tlas_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Matrix stack implementation
#define MAX_MATRIX_STACK_DEPTH 32

struct MatrixStack {
    Matrix4x4 matrices[MAX_MATRIX_STACK_DEPTH];
    int depth;
};

// Draw call record - stores transform at time of draw() call
typedef struct DrawRecord {
    BLASHandle blas_handle;
    Matrix4x4 transform;
    Matrix4x4 inv_transform;
    uint32_t material_id;
    uint32_t instance_id;
} DrawRecord;

// TLAS manager implementation
struct TLASManager {
    MatrixStack matrix_stack;
    
    // Draw call recording
    DrawRecord* draw_records;
    int draw_count;
    int draw_capacity;
    
    // Built TLAS
    TLAS* tlas;
    uint32_t next_instance_id;
    
    // Configuration
    int max_instances;
};

// Matrix stack operations
static void matrix_stack_init(MatrixStack* stack) {
    stack->depth = 0;
    stack->matrices[0] = matrix_identity();
}

static Matrix4x4* matrix_stack_top(MatrixStack* stack) {
    return &stack->matrices[stack->depth];
}

static bool matrix_stack_push(MatrixStack* stack) {
    if (stack->depth >= MAX_MATRIX_STACK_DEPTH - 1) {
        return false; // Stack overflow
    }
    
    stack->depth++;
    stack->matrices[stack->depth] = stack->matrices[stack->depth - 1];
    return true;
}

static bool matrix_stack_pop(MatrixStack* stack) {
    if (stack->depth <= 0) {
        return false; // Stack underflow
    }
    
    stack->depth--;
    return true;
}

TLASManager* tlas_manager_create(int max_instances) {
    TLASManager* manager = malloc(sizeof(TLASManager));
    if (!manager) return NULL;
    
    matrix_stack_init(&manager->matrix_stack);
    
    manager->draw_records = malloc(max_instances * sizeof(DrawRecord));
    if (!manager->draw_records) {
        free(manager);
        return NULL;
    }
    
    manager->draw_count = 0;
    manager->draw_capacity = max_instances;
    manager->tlas = NULL;
    manager->next_instance_id = 1;
    manager->max_instances = max_instances;
    
    return manager;
}

void tlas_manager_destroy(TLASManager* manager) {
    if (!manager) return;
    
    if (manager->tlas) {
        tlas_destroy(manager->tlas);
    }
    
    free(manager->draw_records);
    free(manager);
}

void tlas_manager_push_matrix(TLASManager* manager) {
    if (!manager) return;
    
    if (!matrix_stack_push(&manager->matrix_stack)) {
        printf("Warning: Matrix stack overflow in TLAS manager\n");
    }
}

void tlas_manager_pop_matrix(TLASManager* manager) {
    if (!manager) return;
    
    if (!matrix_stack_pop(&manager->matrix_stack)) {
        printf("Warning: Matrix stack underflow in TLAS manager\n");
    }
}

void tlas_manager_load_identity(TLASManager* manager) {
    if (!manager) return;
    
    *matrix_stack_top(&manager->matrix_stack) = matrix_identity();
}

void tlas_manager_load_matrix(TLASManager* manager, const Matrix4x4* matrix) {
    if (!manager || !matrix) return;
    
    *matrix_stack_top(&manager->matrix_stack) = *matrix;
}

void tlas_manager_multiply_matrix(TLASManager* manager, const Matrix4x4* matrix) {
    if (!manager || !matrix) return;
    
    Matrix4x4* current = matrix_stack_top(&manager->matrix_stack);
    *current = matrix_multiply(current, matrix);
}

void tlas_manager_translate(TLASManager* manager, float x, float y, float z) {
    if (!manager) return;
    
    Matrix4x4 trans = matrix_translation(x, y, z);
    tlas_manager_multiply_matrix(manager, &trans);
}

void tlas_manager_scale(TLASManager* manager, float sx, float sy, float sz) {
    if (!manager) return;
    
    Matrix4x4 scale = matrix_scale(sx, sy, sz);
    tlas_manager_multiply_matrix(manager, &scale);
}

void tlas_manager_rotate_x(TLASManager* manager, float angle_radians) {
    if (!manager) return;
    
    Matrix4x4 rot = matrix_rotation_x(angle_radians);
    tlas_manager_multiply_matrix(manager, &rot);
}

void tlas_manager_rotate_y(TLASManager* manager, float angle_radians) {
    if (!manager) return;
    
    Matrix4x4 rot = matrix_rotation_y(angle_radians);
    tlas_manager_multiply_matrix(manager, &rot);
}

void tlas_manager_rotate_z(TLASManager* manager, float angle_radians) {
    if (!manager) return;
    
    Matrix4x4 rot = matrix_rotation_z(angle_radians);
    tlas_manager_multiply_matrix(manager, &rot);
}

void tlas_manager_rotate_axis(TLASManager* manager, Vec3 axis, float angle_radians) {
    if (!manager) return;
    
    Matrix4x4 rot = matrix_rotation_axis(axis, angle_radians);
    tlas_manager_multiply_matrix(manager, &rot);
}

uint32_t tlas_manager_draw(TLASManager* manager, BLASHandle blas_handle, uint32_t material_id) {
    if (!manager || blas_handle == INVALID_BLAS_HANDLE) return 0;
    
    if (manager->draw_count >= manager->draw_capacity) {
        printf("Warning: TLAS manager draw capacity exceeded\n");
        return 0;
    }
    
    DrawRecord* record = &manager->draw_records[manager->draw_count];
    record->blas_handle = blas_handle;
    record->transform = *matrix_stack_top(&manager->matrix_stack);
    record->inv_transform = matrix_inverse(&record->transform);
    record->material_id = material_id;
    record->instance_id = manager->next_instance_id++;
    
    manager->draw_count++;
    return record->instance_id;
}

void tlas_manager_clear(TLASManager* manager) {
    if (!manager) return;
    
    manager->draw_count = 0;
    manager->next_instance_id = 1;
    
    // Reset matrix stack
    matrix_stack_init(&manager->matrix_stack);
    
    // Clean up existing TLAS
    if (manager->tlas) {
        tlas_destroy(manager->tlas);
        manager->tlas = NULL;
    }
}

void tlas_manager_build(TLASManager* manager, BLASManager* blas_manager) {
    if (!manager || !blas_manager || manager->draw_count == 0) return;
    
    // Clean up existing TLAS
    if (manager->tlas) {
        tlas_destroy(manager->tlas);
    }
    
    // Create new TLAS
    manager->tlas = tlas_create(manager->draw_count);
    if (!manager->tlas) {
        printf("Failed to create TLAS for %d instances\n", manager->draw_count);
        return;
    }
    
    // Convert draw records to BVH instances
    for (int i = 0; i < manager->draw_count; i++) {
        DrawRecord* record = &manager->draw_records[i];
        
        // Get BLAS from manager
        BLAS* blas = blas_manager_get_blas(blas_manager, record->blas_handle);
        if (!blas) {
            printf("Warning: BLAS handle %u not found in BLAS manager\n", record->blas_handle);
            continue;
        }
        
        // Create BVH instance
        BVHInstance* instance = bvh_instance_create(blas, record->instance_id);
        if (!instance) {
            printf("Failed to create BVH instance for draw record %d\n", i);
            continue;
        }
        
        // Set transform
        bvh_instance_set_transform(instance, &record->transform);
        
        // Update BLAS start index from manager
        BLASOffsets offsets = blas_manager_get_offsets(blas_manager, record->blas_handle);
        instance->blas_start_index = offsets.node_offset;
        
        // Add to TLAS
        tlas_add_instance(manager->tlas, instance);
    }
    
    // Build TLAS
    tlas_build(manager->tlas);
}

int tlas_manager_get_instance_count(TLASManager* manager) {
    if (!manager || !manager->tlas) return 0;
    return manager->tlas->instance_count;
}

int tlas_manager_get_node_count(TLASManager* manager) {
    if (!manager || !manager->tlas) return 0;
    return manager->tlas->node_count;
}

void tlas_manager_generate_instance_texture_data(TLASManager* manager, 
                                                BLASManager* blas_manager,
                                                float* output_data, 
                                                int texture_width,
                                                int texture_height) {
    if (!manager || !manager->tlas || !blas_manager || !output_data) return;
    
    for (int i = 0; i < manager->tlas->instance_count; i++) {
        BVHInstance* inst = &manager->tlas->instances[i];
        int baseIdx = i * 4;
        
        // Rows 0-3: transform matrix (4x4)
        for (int row = 0; row < 4; row++) {
            int rowIdx = texture_width * (row * 4) + baseIdx;
            output_data[rowIdx + 0] = inst->transform.m[row * 4 + 0];
            output_data[rowIdx + 1] = inst->transform.m[row * 4 + 1];
            output_data[rowIdx + 2] = inst->transform.m[row * 4 + 2];
            output_data[rowIdx + 3] = inst->transform.m[row * 4 + 3];
        }
        
        // Rows 4-7: inverse transform matrix (4x4)
        for (int row = 0; row < 4; row++) {
            int rowIdx = texture_width * ((row + 4) * 4) + baseIdx;
            output_data[rowIdx + 0] = inst->inv_transform.m[row * 4 + 0];
            output_data[rowIdx + 1] = inst->inv_transform.m[row * 4 + 1];
            output_data[rowIdx + 2] = inst->inv_transform.m[row * 4 + 2];
            output_data[rowIdx + 3] = inst->inv_transform.m[row * 4 + 3];
        }
        
        // Row 8: metadata (blasStartIndex + instanceId + padding)
        int metadataIdx = texture_width * (8 * 4) + baseIdx;
        output_data[metadataIdx + 0] = (float)inst->blas_start_index;
        output_data[metadataIdx + 1] = (float)inst->instance_id;
        output_data[metadataIdx + 2] = 0.0f; // padding
        output_data[metadataIdx + 3] = 0.0f; // padding
    }
}

void tlas_manager_generate_node_texture_data(TLASManager* manager, 
                                            float* output_data,
                                            int texture_width, 
                                            int texture_height) {
    if (!manager || !manager->tlas || !output_data) return;
    
    for (int i = 0; i < manager->tlas->node_count; i++) {
        TLASNode* node = &manager->tlas->nodes[i];
        int baseIdx = i * 4;
        
        // Row 0: aabbMin + leftRight
        output_data[baseIdx + 0] = node->aabb_min.x;
        output_data[baseIdx + 1] = node->aabb_min.y;
        output_data[baseIdx + 2] = node->aabb_min.z;
        output_data[baseIdx + 3] = (float)node->left_right;
        
        // Row 1: aabbMax + blasIndex
        int row1Idx = texture_width * 4 + baseIdx;
        output_data[row1Idx + 0] = node->aabb_max.x;
        output_data[row1Idx + 1] = node->aabb_max.y;
        output_data[row1Idx + 2] = node->aabb_max.z;
        output_data[row1Idx + 3] = (float)node->blas_index;
        
        // Row 2: padding
        int row2Idx = texture_width * 8 + baseIdx;
        output_data[row2Idx + 0] = 0.0f;
        output_data[row2Idx + 1] = 0.0f;
        output_data[row2Idx + 2] = 0.0f;
        output_data[row2Idx + 3] = 0.0f;
    }
}

TLAS* tlas_manager_get_tlas(TLASManager* manager) {
    return manager ? manager->tlas : NULL;
}

void tlas_manager_print_stats(TLASManager* manager) {
    if (!manager) {
        printf("TLAS Manager: NULL\n");
        return;
    }
    
    printf("=== TLAS Manager Statistics ===\n");
    printf("Draw records: %d/%d\n", manager->draw_count, manager->draw_capacity);
    printf("Matrix stack depth: %d/%d\n", manager->matrix_stack.depth, MAX_MATRIX_STACK_DEPTH);
    printf("Next instance ID: %u\n", manager->next_instance_id);
    
    if (manager->tlas) {
        printf("Built TLAS: %d instances, %d nodes\n", 
               manager->tlas->instance_count, manager->tlas->node_count);
    } else {
        printf("TLAS: Not built\n");
    }
}