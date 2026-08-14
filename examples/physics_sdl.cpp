// physics_sdl:skiff Canvas + Box2D 物理模拟预览。
//
// 运行:./physics_sdl         打开窗口,点击画布投放刚体
//      ./physics_sdl --smoke 跑约 1.5 秒自动退出
#include <cstdint>
#include <functional>
#include <string>

#include "app_core/physics_scene.hpp"
#include "app_core/scene_host.hpp"
#include "physics_draw.hpp"
#include "pnd_state.hpp"
#include "skiff/skiff.hpp"
#include "skiff_lvgl.hpp"
#include "skiff_lvgl_sdl3.hpp"

namespace {

const int kWinW = 800;
const int kWinH = 480;
const int kBarH = 48;
const int kHintH = 32;
const int kCanvasW = 800;
const int kCanvasH = kWinH - kBarH - kHintH;
const char* const kFont = "assets/fonts/Hiragino Sans GB.ttc";

skiff::ElementView toolBtn(const char* label, std::function<void()> onTap) {
    return skiff::Button(label, std::move(onTap))
        .size(80, 32)
        .bg(0x2A3340)
        .fg(0xE8EEF7)
        .radius(6)
        .ttf(kFont, 14);
}

void syncPhysicsUi(skiff::components::PageView& page, const app::PhysicsScene& scene) {
    skiff::components::StateView& st = page.stateView();
    st.get<int>(pnd::phys::frame).setIfChanged((int)scene.frame());
    st.get<bool>(pnd::phys::paused).setIfChanged(scene.paused());
    st.get<int>(pnd::phys::shape).setIfChanged(scene.dropCircle() ? 1 : 0);
}

} // namespace

int main(int argc, char** argv) {
    const bool smoke = (argc > 1 && std::string(argv[1]) == "--smoke");

    lv_init();
    skiff::lvgl::createSdl3Display(kWinW, kWinH, "skiff canvas + Box2D");

    app::PhysicsScene scene(kCanvasW, kCanvasH);
    app::SceneHost host;
    host.add(scene);
    host.activate("physics");

    skiff::components::PageView page("physics", {
        skiff::components::state::of<int>(pnd::phys::frame, 0),
        skiff::components::state::of<bool>(pnd::phys::paused, false),
        skiff::components::state::of<int>(pnd::phys::shape, 0),
    });
    scene.onChange([&page, &scene] { syncPhysicsUi(page, scene); });

    skiff::lvgl::LvglBackend backend(lv_scr_act());
    skiff::App app(backend, [&page, &scene]() -> skiff::Element {
        return page.render([&scene](skiff::components::StateView& st) -> skiff::Element {
            skiff::State<int>& frame = st.get<int>(pnd::phys::frame);
            skiff::State<bool>& paused = st.get<bool>(pnd::phys::paused);
            skiff::State<int>& shape = st.get<int>(pnd::phys::shape);

            return skiff::VStack({
                skiff::HStack({
                    skiff::Text("Box2D").ttf(kFont, 20).fg(0xE8EEF7),
                    skiff::Spacer(),
                    skiff::Watch(paused, [&scene](bool p) -> skiff::Element {
                        return toolBtn(p ? "继续" : "暂停",
                                       [&scene] { scene.togglePaused(); });
                    }),
                    skiff::Watch(shape, [&scene](int s) -> skiff::Element {
                        return toolBtn(s == 0 ? "投方块" : "投圆球",
                                       [&scene] {
                                           scene.setDropCircle(!scene.dropCircle());
                                       });
                    }),
                    toolBtn("重置", [&scene] { scene.reset(); }),
                }, 8).widthPct(100).size(0, kBarH).pad(8).bg(0x12161F),

                skiff::Watch(frame, [&scene](int) -> skiff::Element {
                    return skiff::Canvas(kCanvasW, kCanvasH,
                                         [&scene](skiff::CanvasContext& c) {
                                             pnd::physics::paintScene(c, scene);
                                         })
                        .onTapAt([&scene](int x, int y) {
                            scene.spawnAtCanvas(x, y);
                        });
                }),

                skiff::Text("点击画布投放刚体  ·  click to drop")
                    .ttf(kFont, 14)
                    .fg(0x8A93A6)
                    .size(kCanvasW, kHintH)
                    .padLeft(12),
            }, 0).size(kWinW, kWinH).bg(0x0E1116);
        });
    });
    page.bind(app);
    app.start();

    uint32_t last = lv_tick_get();
    int frames = 0;
    while (skiff::lvgl::sdl3Pump()) {
        const uint32_t now = lv_tick_get();
        float dt = (now - last) * 0.001f;
        last = now;
        if (dt < 0.0f) dt = 0.0f;
        if (dt > 0.25f) dt = 0.25f;
        host.tick(dt);

        lv_timer_handler();
        app.update();
        if (smoke && ++frames >= 90) break;
    }

    skiff::lvgl::destroySdl3Display();
    return 0;
}
