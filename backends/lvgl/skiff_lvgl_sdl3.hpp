// SDL3 宿主:给 LVGL 后端提供 PC 窗口(显示 + 鼠标),用于开发期即时预览。
//
// 注意分层:SDL3 只做 LVGL 的显示/输入驱动,渲染仍由 LVGL 完成,
// DSL 页面代码不感知 SDL3 的存在。仅用于 PC 端,嵌入式固件不编这个文件。
#pragma once

#include "lvgl.h"

namespace skiff {
class App;
class Platform;
}

namespace skiff {
namespace lvgl {

// 创建 SDL3 窗口并注册为 LVGL 显示 + 指针输入设备。
lv_disp_t* createSdl3Display(int horRes, int verRes, const char* title);

// 处理 SDL 事件、推进 LVGL tick、做帧间隔;窗口被关闭时返回 false。
bool sdl3Pump();

// 运行主循环:处理 SDL 事件 + 驱动 LVGL + 刷新 App。
// 页面代码不再直接调用 lv_timer_handler()。
// 传入 platform 时,每帧先 pumpEvents(平台→UI 事件),再 lv_timer_handler,
// 再 app.update(),最后 pumpDeferred(点击里排队的 playMusic 等)。
void run(skiff::App& app, skiff::Platform* platform = nullptr);

void destroySdl3Display();

} // namespace lvgl
} // namespace skiff
