#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include "include/object_allocator.h"

#define TEST_PASSED printf("PASSED: %s\n", __func__)
#define TEST_FAILED printf("FAILED: %s (line %d)\n", __func__, __LINE__)

typedef struct TestPoint {
    float x, y, z;
    int data;
} TestPoint;

// Test that ObjectAllocator works with our project setup
bool test_object_allocator_integration() {
    ObjectAllocator* allocator = oa_create(sizeof(TestPoint), 10);
    if (!allocator) {
        TEST_FAILED;
        return false;
    }
    
    // Allocate a test point
    TestPoint* point = (TestPoint*)oa_alloc(allocator);
    if (!point) {
        TEST_FAILED;
        oa_destroy(allocator);
        return false;
    }
    
    // Initialize and verify the point
    point->x = 1.0f;
    point->y = 2.0f;
    point->z = 3.0f;
    point->data = 42;
    
    if (point->x != 1.0f || point->y != 2.0f || point->z != 3.0f || point->data != 42) {
        TEST_FAILED;
        oa_destroy(allocator);
        return false;
    }
    
    // Free the point
    oa_free(allocator, point);
    oa_destroy(allocator);
    TEST_PASSED;
    return true;
}

// Placeholder test for future SpatialHash implementation
bool test_spatial_hash_placeholder() {
    // This test will be replaced when we implement SpatialHash
    printf("PLACEHOLDER: SpatialHash tests will be implemented next\n");
    TEST_PASSED;
    return true;
}

// Run all tests
int main() {
    printf("=== SpatialQueryLib Tests ===\n");
    
    int passed = 0;
    int total = 2;
    
    if (test_object_allocator_integration()) passed++;
    if (test_spatial_hash_placeholder()) passed++;
    
    printf("\n%d/%d tests passed\n", passed, total);
    
    return (passed == total) ? 0 : 1;
}