// physics_sdl:skiff Canvas + Box2D 物理模拟预览。
//
// 运行:./physics_sdl         打开窗口,点击画布投放刚体
//      ./physics_sdl --smoke 跑约 1.5 秒自动退出
#include <cstdint>
#include <functional>
#include <string>

#include "physics_sim.hpp"
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

} // namespace

int main(int argc, char** argv) {
    const bool smoke = (argc > 1 && std::string(argv[1]) == "--smoke");

    lv_init();
    skiff::lvgl::createSdl3Display(kWinW, kWinH, "skiff canvas + Box2D");

    pnd::physics::Sim sim(kCanvasW, kCanvasH);
    skiff::components::PageView page("physics", {
        skiff::components::state::of<int>("frame", 0),
        skiff::components::state::of<bool>("paused", false),
        skiff::components::state::of<int>("shape", 0),
    });

    skiff::lvgl::LvglBackend backend(lv_scr_act());
    skiff::App app(backend, [&page, &sim]() -> skiff::Element {
        return page.render([&sim](skiff::components::StateView& st) -> skiff::Element {
            skiff::State<int>& frame = st.get<int>("frame");
            skiff::State<bool>& paused = st.get<bool>("paused");
            skiff::State<int>& shape = st.get<int>("shape");

            return skiff::VStack({
                skiff::HStack({
                    skiff::Text("Box2D").ttf(kFont, 20).fg(0xE8EEF7),
                    skiff::Spacer(),
                    skiff::Watch(paused, [&paused](bool p) -> skiff::Element {
                        return toolBtn(p ? "继续" : "暂停",
                                       [&paused] { paused.set(!paused.get()); });
                    }),
                    skiff::Watch(shape, [&shape](int s) -> skiff::Element {
                        return toolBtn(s == 0 ? "投方块" : "投圆球",
                                       [&shape, s] { shape.set(s == 0 ? 1 : 0); });
                    }),
                    toolBtn("重置", [&sim, &frame] {
                        sim.reset();
                        frame.set(frame.get() + 1);
                    }),
                }, 8).widthPct(100).size(0, kBarH).pad(8).bg(0x12161F),

                skiff::Watch(frame, [&sim, &frame, &shape](int) -> skiff::Element {
                    return skiff::Canvas(kCanvasW, kCanvasH,
                                         [&sim](skiff::CanvasContext& c) {
                                             sim.paint(c);
                                         })
                        .onTapAt([&sim, &frame, &shape](int x, int y) {
                            sim.spawnAtCanvas(x, y, shape.get() != 0);
                            frame.set(frame.get() + 1);
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
    float acc = 0.0f;
    const float dt = 1.0f / 60.0f;
    int frames = 0;
    while (skiff::lvgl::sdl3Pump()) {
        const uint32_t now = lv_tick_get();
        acc += (now - last) * 0.001f;
        last = now;
        if (acc > 0.25f) acc = 0.25f;

        skiff::State<bool>& paused = page.stateView().get<bool>("paused");
        skiff::State<int>& frame = page.stateView().get<int>("frame");
        bool dirty = false;
        while (acc >= dt) {
            acc -= dt;
            if (!paused.get()) {
                sim.step(dt);
                dirty = true;
            }
        }
        if (dirty) frame.set(frame.get() + 1);

        lv_timer_handler();
        app.update();
        if (smoke && ++frames >= 90) break;
    }

    skiff::lvgl::destroySdl3Display();
    return 0;
}
