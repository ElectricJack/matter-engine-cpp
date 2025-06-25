#include "../include/blas_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Hash table for deduplicating BLAS based on mesh data
#define BLAS_HASH_TABLE_SIZE 256

// BLAS registry entry
typedef struct BLASEntry {
    BLASHandle handle;
    BLAS* blas;
    Triangle* triangles;
    int triangle_count;
    uint32_t hash;
    struct BLASEntry* next;
} BLASEntry;

// BLAS manager implementation
struct BLASManager {
    BLASEntry* hash_table[BLAS_HASH_TABLE_SIZE];
    BLASEntry** entries;           // Array of all entries for iteration
    int entry_count;
    int entry_capacity;
    BLASHandle next_handle;
    
    // Cached totals for GPU texture generation
    int total_triangle_count;
    int total_node_count;
    bool totals_dirty;
};

// Simple hash function for triangle data
static uint32_t hash_triangles(Triangle* triangles, int count) {
    uint32_t hash = 2166136261u; // FNV-1a offset basis
    
    for (int i = 0; i < count; i++) {
        // Hash vertex positions only (ignore normals/materials for deduplication)
        float* data = (float*)&triangles[i];
        for (int j = 0; j < 9; j++) { // 3 vertices * 3 components each
            uint32_t val = *(uint32_t*)&data[j];
            hash ^= val;
            hash *= 16777619u; // FNV-1a prime
        }
    }
    
    return hash;
}

// Check if two triangle arrays are equal
static bool triangles_equal(Triangle* a, Triangle* b, int count) {
    for (int i = 0; i < count; i++) {
        if (memcmp(&a[i].v0, &b[i].v0, sizeof(Vec3) * 3) != 0) {
            return false;
        }
    }
    return true;
}

BLASManager* blas_manager_create(void) {
    BLASManager* manager = malloc(sizeof(BLASManager));
    if (!manager) return NULL;
    
    memset(manager->hash_table, 0, sizeof(manager->hash_table));
    
    manager->entries = malloc(16 * sizeof(BLASEntry*));
    if (!manager->entries) {
        free(manager);
        return NULL;
    }
    
    manager->entry_count = 0;
    manager->entry_capacity = 16;
    manager->next_handle = 1; // Start at 1, 0 is reserved for INVALID_BLAS_HANDLE
    manager->total_triangle_count = 0;
    manager->total_node_count = 0;
    manager->totals_dirty = true;
    
    return manager;
}

void blas_manager_destroy(BLASManager* manager) {
    if (!manager) return;
    
    // Cleanup all entries
    for (int i = 0; i < manager->entry_count; i++) {
        BLASEntry* entry = manager->entries[i];
        if (entry->blas) {
            blas_destroy(entry->blas);
        }
        free(entry->triangles);
        free(entry);
    }
    
    free(manager->entries);
    free(manager);
}

static void update_totals(BLASManager* manager) {
    if (!manager->totals_dirty) return;
    
    manager->total_triangle_count = 0;
    manager->total_node_count = 0;
    
    for (int i = 0; i < manager->entry_count; i++) {
        BLASEntry* entry = manager->entries[i];
        if (entry->blas) {
            manager->total_triangle_count += entry->blas->triangle_count;
            manager->total_node_count += entry->blas->node_count;
        }
    }
    
    manager->totals_dirty = false;
}

BLASHandle blas_manager_register_triangles(BLASManager* manager, 
                                         Triangle* triangles, 
                                         int triangle_count,
                                         int max_triangles_per_leaf) {
    if (!manager || !triangles || triangle_count <= 0) {
        return INVALID_BLAS_HANDLE;
    }
    
    // Calculate hash for deduplication
    uint32_t hash = hash_triangles(triangles, triangle_count);
    int bucket = hash % BLAS_HASH_TABLE_SIZE;
    
    // Check if BLAS already exists
    BLASEntry* existing = manager->hash_table[bucket];
    while (existing) {
        if (existing->hash == hash && 
            existing->triangle_count == triangle_count &&
            triangles_equal(existing->triangles, triangles, triangle_count)) {
            return existing->handle;
        }
        existing = existing->next;
    }
    
    // Create new BLAS entry
    BLASEntry* entry = malloc(sizeof(BLASEntry));
    if (!entry) return INVALID_BLAS_HANDLE;
    
    // Copy triangle data
    entry->triangles = malloc(triangle_count * sizeof(Triangle));
    if (!entry->triangles) {
        free(entry);
        return INVALID_BLAS_HANDLE;
    }
    memcpy(entry->triangles, triangles, triangle_count * sizeof(Triangle));
    
    // Create BLAS
    entry->blas = blas_create(entry->triangles, triangle_count, max_triangles_per_leaf);
    if (!entry->blas) {
        free(entry->triangles);
        free(entry);
        return INVALID_BLAS_HANDLE;
    }
    
    blas_build(entry->blas);
    
    entry->handle = manager->next_handle++;
    entry->triangle_count = triangle_count;
    entry->hash = hash;
    
    // Add to hash table
    entry->next = manager->hash_table[bucket];
    manager->hash_table[bucket] = entry;
    
    // Add to entries array
    if (manager->entry_count >= manager->entry_capacity) {
        manager->entry_capacity *= 2;
        manager->entries = realloc(manager->entries, 
                                  manager->entry_capacity * sizeof(BLASEntry*));
        if (!manager->entries) {
            // This is bad - we're in an inconsistent state
            // In production code, should handle this more gracefully
            free(entry->triangles);
            blas_destroy(entry->blas);
            free(entry);
            return INVALID_BLAS_HANDLE;
        }
    }
    
    manager->entries[manager->entry_count++] = entry;
    manager->totals_dirty = true;
    
    return entry->handle;
}

bool blas_manager_has_blas(BLASManager* manager, BLASHandle handle) {
    if (!manager || handle == INVALID_BLAS_HANDLE) return false;
    
    for (int i = 0; i < manager->entry_count; i++) {
        if (manager->entries[i]->handle == handle) {
            return true;
        }
    }
    return false;
}

BLAS* blas_manager_get_blas(BLASManager* manager, BLASHandle handle) {
    if (!manager || handle == INVALID_BLAS_HANDLE) return NULL;
    
    for (int i = 0; i < manager->entry_count; i++) {
        if (manager->entries[i]->handle == handle) {
            return manager->entries[i]->blas;
        }
    }
    return NULL;
}

int blas_manager_get_total_triangle_count(BLASManager* manager) {
    if (!manager) return 0;
    update_totals(manager);
    return manager->total_triangle_count;
}

int blas_manager_get_total_node_count(BLASManager* manager) {
    if (!manager) return 0;
    update_totals(manager);
    return manager->total_node_count;
}

BLASOffsets blas_manager_get_offsets(BLASManager* manager, BLASHandle handle) {
    BLASOffsets offsets = {0, 0};
    if (!manager || handle == INVALID_BLAS_HANDLE) return offsets;
    
    int triangle_offset = 0;
    int node_offset = 0;
    
    for (int i = 0; i < manager->entry_count; i++) {
        BLASEntry* entry = manager->entries[i];
        if (entry->handle == handle) {
            offsets.triangle_offset = triangle_offset;
            offsets.node_offset = node_offset;
            return offsets;
        }
        
        if (entry->blas) {
            triangle_offset += entry->blas->triangle_count;
            node_offset += entry->blas->node_count;
        }
    }
    
    return offsets; // Not found
}

void blas_manager_generate_triangle_texture_data(BLASManager* manager, Triangle* output_triangles) {
    if (!manager || !output_triangles) return;
    
    int offset = 0;
    for (int i = 0; i < manager->entry_count; i++) {
        BLASEntry* entry = manager->entries[i];
        if (entry->blas) {
            memcpy(&output_triangles[offset], entry->triangles, 
                   entry->blas->triangle_count * sizeof(Triangle));
            offset += entry->blas->triangle_count;
        }
    }
}

void blas_manager_generate_node_texture_data(BLASManager* manager, BVHNode* output_nodes) {
    if (!manager || !output_nodes) return;
    
    int node_offset = 0;
    int triangle_offset = 0;
    
    for (int i = 0; i < manager->entry_count; i++) {
        BLASEntry* entry = manager->entries[i];
        if (entry->blas) {
            // Copy nodes and adjust indices
            for (int j = 0; j < entry->blas->node_count; j++) {
                output_nodes[node_offset + j] = entry->blas->nodes[j];
                
                if (output_nodes[node_offset + j].tri_count > 0) {
                    // Leaf node - adjust triangle indices
                    output_nodes[node_offset + j].left_first += triangle_offset;
                } else {
                    // Internal node - adjust child node indices
                    output_nodes[node_offset + j].left_first += node_offset;
                }
            }
            
            node_offset += entry->blas->node_count;
            triangle_offset += entry->blas->triangle_count;
        }
    }
}

int blas_manager_get_unique_blas_count(BLASManager* manager) {
    return manager ? manager->entry_count : 0;
}

void blas_manager_print_stats(BLASManager* manager) {
    if (!manager) {
        printf("BLAS Manager: NULL\n");
        return;
    }
    
    update_totals(manager);
    
    printf("=== BLAS Manager Statistics ===\n");
    printf("Unique BLAS count: %d\n", manager->entry_count);
    printf("Total triangles: %d\n", manager->total_triangle_count);
    printf("Total nodes: %d\n", manager->total_node_count);
    printf("Next handle: %u\n", manager->next_handle);
    
    // Distribution across hash buckets
    int used_buckets = 0;
    int max_chain_length = 0;
    for (int i = 0; i < BLAS_HASH_TABLE_SIZE; i++) {
        int chain_length = 0;
        BLASEntry* entry = manager->hash_table[i];
        while (entry) {
            chain_length++;
            entry = entry->next;
        }
        if (chain_length > 0) {
            used_buckets++;
            if (chain_length > max_chain_length) {
                max_chain_length = chain_length;
            }
        }
    }
    
    printf("Hash table: %d/%d buckets used, max chain length: %d\n", 
           used_buckets, BLAS_HASH_TABLE_SIZE, max_chain_length);
}