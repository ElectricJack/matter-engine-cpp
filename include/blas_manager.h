#ifndef BLAS_MANAGER_H
#define BLAS_MANAGER_H

#include "bvh.h"
#include <stdint.h>
#include <stdbool.h>

// BLAS handle for tracking unique BLAS instances
typedef uint32_t BLASHandle;
#define INVALID_BLAS_HANDLE 0

// BLAS manager structure
typedef struct BLASManager BLASManager;

// BLAS creation interface
BLASManager* blas_manager_create(void);
void blas_manager_destroy(BLASManager* manager);

// Register mesh data and get BLAS handle
// Returns INVALID_BLAS_HANDLE on failure
BLASHandle blas_manager_register_triangles(BLASManager* manager, 
                                          Triangle* triangles, 
                                          int triangle_count,
                                          int max_triangles_per_leaf);

// Check if a BLAS exists
bool blas_manager_has_blas(BLASManager* manager, BLASHandle handle);

// Get BLAS from handle
BLAS* blas_manager_get_blas(BLASManager* manager, BLASHandle handle);

// Get total counts for GPU texture generation
int blas_manager_get_total_triangle_count(BLASManager* manager);
int blas_manager_get_total_node_count(BLASManager* manager);

// GPU texture generation
// Returns offset into combined arrays where this BLAS data starts
typedef struct {
    int triangle_offset;
    int node_offset;
} BLASOffsets;

// Get offsets for a specific BLAS in the combined arrays
BLASOffsets blas_manager_get_offsets(BLASManager* manager, BLASHandle handle);

// Generate combined triangle array for GPU upload
// User must provide pre-allocated buffer of size get_total_triangle_count()
void blas_manager_generate_triangle_texture_data(BLASManager* manager, Triangle* output_triangles);

// Generate combined node array for GPU upload  
// User must provide pre-allocated buffer of size get_total_node_count()
void blas_manager_generate_node_texture_data(BLASManager* manager, BVHNode* output_nodes);

// Statistics and debugging
int blas_manager_get_unique_blas_count(BLASManager* manager);
void blas_manager_print_stats(BLASManager* manager);

#endif // BLAS_MANAGER_H