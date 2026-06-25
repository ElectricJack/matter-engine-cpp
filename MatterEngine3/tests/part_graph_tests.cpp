#include "part_graph.h"
#include <cstdio>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); ++failures; } } while (0)

int main() {
    using namespace part_graph;
    // Empty params canonicalize to the empty string.
    CHECK(serialize_params(Params{}) == "", "empty params -> empty string");

    if (failures == 0) printf("All part_graph tests passed\n");
    return failures == 0 ? 0 : 1;
}
