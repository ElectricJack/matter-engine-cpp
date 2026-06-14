#include "../include/lattice.h"
#include "../include/occupancy.h"
#include <cstdio>
#include <cmath>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); ++failures; } } while (0)

static void test_grid_lattice() {
    GridLattice lat(2.0f);
    Vector3 p = lat.slot_position(SlotCoord{3, 0, -1});
    CHECK(fabsf(p.x - 6.0f) < 1e-6f, "grid slot_position x scales by spacing");
    CHECK(fabsf(p.y - 0.0f) < 1e-6f, "grid slot_position y scales by spacing");
    CHECK(fabsf(p.z + 2.0f) < 1e-6f, "grid slot_position z scales by spacing");

    const auto& n = lat.neighbor_offsets();
    CHECK(n.size() == 6, "grid has 6 face neighbors");
    bool has_plus_x = false;
    for (const auto& o : n) if (o.x == 1 && o.y == 0 && o.z == 0) has_plus_x = true;
    CHECK(has_plus_x, "grid neighbors include +x");
    CHECK(fabsf(lat.spacing() - 2.0f) < 1e-6f, "grid reports its spacing");
}

static void test_occupancy() {
    Occupancy occ;
    CHECK(occ.count() == 0, "empty occupancy has count 0");
    CHECK(!occ.occupied(SlotCoord{0,0,0}), "unset slot not occupied");

    occ.set(SlotCoord{2, -3, 5}, SlotData{7});
    occ.set(SlotCoord{2, -3, 5}, SlotData{42});  // overwrite with different materialId
    CHECK(occ.count() == 1, "re-setting same slot does not grow count");
    CHECK(occ.occupied(SlotCoord{2, -3, 5}), "set slot is occupied");
    CHECK(!occ.occupied(SlotCoord{2, -3, 6}), "neighbor slot not occupied");

    // Verify overwrite actually stored the new materialId (42, not the old 7).
    int overwritten_id = -1;
    occ.for_each([&](SlotCoord c, const SlotData& d) {
        if (c.x == 2 && c.y == -3 && c.z == 5) overwritten_id = d.materialId;
    });
    CHECK(overwritten_id == 42, "overwrite stores new materialId, not old one");

    // pack/unpack round-trips negatives via for_each.
    bool seen = false;
    occ.for_each([&](SlotCoord c, const SlotData& d) {
        if (c.x == 2 && c.y == -3 && c.z == 5 && d.materialId == 42) seen = true;
    });
    CHECK(seen, "for_each round-trips coords (incl. negatives) and data");
}

int main() {
    test_grid_lattice();
    test_occupancy();
    if (failures == 0) printf("All particle_culling tests passed\n");
    return failures == 0 ? 0 : 1;
}
