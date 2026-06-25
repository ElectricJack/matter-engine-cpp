// Headless SP-5 dev live-edit tests. plain main(), no GL. Drives FakeWatcher +
// fake SP-2/3/4 seams. See docs/superpowers/specs/2026-06-24-dev-live-edit-design.md
#include "file_watcher.h"
#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); ++g_failures; } \
    else { std::printf("  ok: %s\n", msg); } } while (0)

using namespace live_edit;

static void test_fake_watcher_roundtrip() {
    std::printf("[test_fake_watcher_roundtrip]\n");
    FakeWatcher w;
    w.add_watch("/tmp/ObjectSchemas");
    w.set_now_ms(1000);
    w.push("/tmp/ObjectSchemas/rock.js", FileEventKind::Modified);
    std::vector<FileEvent> out;
    int n = w.poll(out);
    CHECK(n == 1, "poll returns one pushed event");
    CHECK(out.size() == 1 && out[0].path == "/tmp/ObjectSchemas/rock.js", "event path round-trips");
    CHECK(out[0].t_ms == 1000, "event stamped with fake clock");
    std::vector<FileEvent> out2;
    CHECK(w.poll(out2) == 0, "second poll drains to empty");
}

int main() {
    std::printf("=== dev_live_edit_tests ===\n");
    test_fake_watcher_roundtrip();
    if (g_failures) { std::printf("\n%d FAILURES\n", g_failures); return 1; }
    std::printf("\nALL PASS\n");
    return 0;
}
