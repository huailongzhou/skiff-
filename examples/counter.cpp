// counter:skiff + LVGL 后端的端到端冒烟示例。
// 在宿主机上用无显示屏幕跑通 "渲染 → 点击 → 状态驱动重建" 全链路。
//
// 页面部分(下面 lambda 里的内容)完全不包含 LVGL 字样——
// 换成 PC 端预览后端时,这段代码不用动。
#include <cstdio>
#include <string>

#include "skiff/skiff.hpp"
#include "skiff_lvgl.hpp"

int main() {
    lv_init();
    skiff::lvgl::createHeadlessDisplay(240, 240);

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

    lv_obj_t* col = lv_obj_get_child(lv_scr_act(), 0);
    std::printf("[初始  ] %s\n", lv_label_get_text(lv_obj_get_child(col, 0)));

    // 模拟一次按钮点击:回调改 State → App 标记失效 → update() 重建
    lv_event_send(lv_obj_get_child(col, 1), LV_EVENT_CLICKED, nullptr);
    lv_timer_handler();
    app.update();

    // 重建后旧指针已失效,重新取一遍
    col = lv_obj_get_child(lv_scr_act(), 0);
    const char* text = lv_label_get_text(lv_obj_get_child(col, 0));
    std::printf("[点击后] %s\n", text);

    const bool ok = std::string(text) == "count = 1";
    std::printf("%s\n", ok ? "OK: 声明式链路全通" : "FAIL: 文本不符合预期");
    return ok ? 0 : 1;
}
