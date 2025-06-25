#ifndef TLAS_MANAGER_H
#define TLAS_MANAGER_H

#include "bvh.h"
#include "blas_manager.h"
#include <stdint.h>
#include <stdbool.h>

// Forward declaration
typedef struct TLASManager TLASManager;

// Matrix stack for hierarchical transformations
typedef struct MatrixStack MatrixStack;

// TLAS manager creation/destruction
TLASManager* tlas_manager_create(int max_instances);
void tlas_manager_destroy(TLASManager* manager);

// Matrix stack operations - similar to OpenGL matrix stack
void tlas_manager_push_matrix(TLASManager* manager);
void tlas_manager_pop_matrix(TLASManager* manager);
void tlas_manager_load_identity(TLASManager* manager);
void tlas_manager_load_matrix(TLASManager* manager, const Matrix4x4* matrix);
void tlas_manager_multiply_matrix(TLASManager* manager, const Matrix4x4* matrix);

// Transformation convenience functions
void tlas_manager_translate(TLASManager* manager, float x, float y, float z);
void tlas_manager_scale(TLASManager* manager, float sx, float sy, float sz);
void tlas_manager_rotate_x(TLASManager* manager, float angle_radians);
void tlas_manager_rotate_y(TLASManager* manager, float angle_radians);
void tlas_manager_rotate_z(TLASManager* manager, float angle_radians);
void tlas_manager_rotate_axis(TLASManager* manager, Vec3 axis, float angle_radians);

// Drawing operations - records instances with current transform
// Returns instance ID for the draw call, or 0 on failure
uint32_t tlas_manager_draw(TLASManager* manager, BLASHandle blas_handle, uint32_t material_id);

// Clear all recorded instances (for new frame)
void tlas_manager_clear(TLASManager* manager);

// Build TLAS from recorded instances (call after all draw() calls)
void tlas_manager_build(TLASManager* manager, BLASManager* blas_manager);

// GPU texture generation  
int tlas_manager_get_instance_count(TLASManager* manager);
int tlas_manager_get_node_count(TLASManager* manager);

// Generate texture data for GPU upload
// User must provide pre-allocated buffers
void tlas_manager_generate_instance_texture_data(TLASManager* manager, 
                                                BLASManager* blas_manager,
                                                float* output_data, 
                                                int texture_width,
                                                int texture_height);

void tlas_manager_generate_node_texture_data(TLASManager* manager, 
                                            float* output_data,
                                            int texture_width, 
                                            int texture_height);

// Get underlying TLAS for compatibility with existing code
TLAS* tlas_manager_get_tlas(TLASManager* manager);

// Statistics and debugging
void tlas_manager_print_stats(TLASManager* manager);

#endif // TLAS_MANAGER_H