// counter_sdl:与 examples/counter.cpp 完全相同的 DSL 页面,
// 跑在 SDL3 窗口里(LVGL 渲染)——开发期即时预览用。
//
// 运行:./counter_sdl         打开窗口,鼠标点 +1
//      ./counter_sdl --smoke 跑 30 帧自动退出(冒烟测试)
#include <string>

#include "skiff/skiff.hpp"
#include "skiff_lvgl.hpp"
#include "skiff_lvgl_sdl3.hpp"

int main(int argc, char** argv) {
    const bool smoke = (argc > 1 && std::string(argv[1]) == "--smoke");

    lv_init();
    skiff::lvgl::createSdl3Display(480, 320, "skiff preview (LVGL + SDL3)");

    skiff::State<int> count(0);

    skiff::lvgl::LvglBackend backend(lv_scr_act());
    skiff::App app(backend, [&count]() -> skiff::Element {
        return skiff::VStack({
            skiff::Text("count = " + std::to_string(count.get())),
            skiff::Button("+1", [&count] { count.set(count.get() + 1); }),
        }, 8);
    });
    app.bind(count);
    app.start();

    int frames = 0;
    while (skiff::lvgl::sdl3Pump()) {
        lv_timer_handler();
        app.update();
        if (smoke && ++frames >= 30) break;
    }

    skiff::lvgl::destroySdl3Display();
    return 0;
}
