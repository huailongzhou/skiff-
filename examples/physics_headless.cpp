// physics_headless:无 UI 跑物理场景,证明应用核可独立运行。
//
// 运行:./build/physics_headless
#include <cstdio>

#include "app_core/physics_scene.hpp"

int main() {
    app::PhysicsScene scene(800, 400);

    scene.setPaused(true);
    scene.tick(1.0f);
    if (scene.frame() != 0) {
        std::fprintf(stderr, "FAIL: paused scene should not step\n");
        return 1;
    }

    scene.setPaused(false);
    const int bodies0 = scene.bodyCount();
    if (bodies0 < 10) {
        std::fprintf(stderr, "FAIL: expected a populated world, bodies=%d\n",
                     bodies0);
        return 1;
    }

    for (int i = 0; i < 120; ++i) scene.tick(1.0f / 60.0f);
    if (scene.frame() == 0) {
        std::fprintf(stderr, "FAIL: running scene should bump frame\n");
        return 1;
    }

    scene.spawnAtCanvas(400, 80);
    const int bodies1 = scene.bodyCount();
    if (bodies1 <= bodies0) {
        std::fprintf(stderr, "FAIL: spawn should add a body (%d -> %d)\n",
                     bodies0, bodies1);
        return 1;
    }

    scene.reset();
    if (scene.bodyCount() != bodies0) {
        std::fprintf(stderr, "FAIL: reset should restore initial body count\n");
        return 1;
    }

    std::printf("OK: physics_headless bodies=%d frame=%llu\n",
                bodies0, (unsigned long long)scene.frame());
    return 0;
}
