// pnd_sdl:车机 PND 风格主页 —— DSL 编写,LVGL 渲染,SDL3 开窗。
//
//   ./pnd_sdl   打开 800x480 窗口,磁贴可点击切换子页
//
// 页面代码只包含 skiff 核心头文件,不感知 LVGL / SDL3。
#include <ctime>
#include <string>

#include "skiff/skiff.hpp"
#include "skiff_lvgl.hpp"
#include "skiff_lvgl_sdl3.hpp"

using skiff::Element;
using skiff::State;

namespace {

// ---- 图标:LVGL 内置符号字体的 UTF-8 字符串 ----
const char* const ICON_GPS   = "\xEF\x84\xA4";
const char* const ICON_CALL  = "\xEF\x82\x95";
const char* const ICON_PLAY  = "\xEF\x81\x8B";
const char* const ICON_AUDIO = "\xEF\x80\x81";
const char* const ICON_VIDEO = "\xEF\x80\x88";
const char* const ICON_IMAGE = "\xEF\x80\xBE";
const char* const ICON_SETUP = "\xEF\x80\x93";
const char* const ICON_WIFI  = "\xEF\x87\xAB";
const char* const ICON_BT    = "\xEF\x8A\x93";
const char* const ICON_BATT  = "\xEF\x89\x81";

// ---- 配色 ----
const uint32_t kBg    = 0x0F141A;  // 页面底色(深蓝黑)
const uint32_t kTile  = 0x26303B;  // 普通磁贴
const uint32_t kNavi  = 0x1565D8;  // 导航蓝
const uint32_t kMusic = 0xD84315;  // 音乐橙红
const uint32_t kPhone = 0x2E7D32;  // 电话绿
const uint32_t kHi    = 0xFFFFFF;
const uint32_t kLo    = 0x9AA4B0;

// 矢量中文字体(FreeType 渲染,任意字号);运行目录需为项目根
// 注意:default.ttf 是裁剪子集,缺很多中文字形;Hiragino Sans GB 覆盖完整
const char* const kFont = "assets/fonts/Hiragino Sans GB.ttc";

std::string clockText() {
    const std::time_t t = std::time(nullptr);
    char buf[8];
    std::strftime(buf, sizeof(buf), "%H:%M", std::localtime(&t));
    return buf;
}

Element bigTile(const char* icon, const char* label, int w, int h,
                uint32_t color, State<std::string>& page) {
    const std::string name = label;
    return skiff::Button({
            skiff::Text(icon).font(32).fg(kHi),
            skiff::Text(label).ttf(kFont, 28).fg(kHi),
        }, [&page, name] { page.set(name); })
        .size(w, h).bg(color);
}

Element smallTile(const char* icon, const char* label, int w, int h,
                  State<std::string>& page) {
    const std::string name = label;
    return skiff::Button({
            skiff::Text(icon).font(24).fg(kHi),
            skiff::Text(label).ttf(kFont, 18).fg(kHi),
        }, [&page, name] { page.set(name); })
        .size(w, h).bg(kTile);
}

Element statusBar() {
    return skiff::HStack({
        skiff::Text(clockText()).font(24).fg(kHi),
        skiff::Spacer(),
        skiff::Text(ICON_GPS).font(20).fg(kLo),
        skiff::Text(ICON_BT).font(20).fg(kLo),
        skiff::Text(ICON_WIFI).font(20).fg(kLo),
        skiff::Text(ICON_BATT).font(20).fg(kLo),
    }, 14).size(776, 32).centered();
}

Element homePage(State<std::string>& page) {
    Element mainRow = skiff::HStack({
        bigTile(ICON_GPS, "导航", 510, 300, kNavi, page),
        skiff::VStack({
            bigTile(ICON_PLAY, "音乐", 256, 145, kMusic, page),
            bigTile(ICON_CALL, "蓝牙电话", 256, 145, kPhone, page),
        }, 10),
    }, 10);

    Element bottomRow = skiff::HStack({
        smallTile(ICON_AUDIO, "收音机", 188, 86, page),
        smallTile(ICON_VIDEO, "视频",   188, 86, page),
        smallTile(ICON_IMAGE, "图片",   188, 86, page),
        smallTile(ICON_SETUP, "设置",   188, 86, page),
    }, 8);

    return skiff::VStack({
        statusBar(),
        skiff::Spacer(),
        mainRow,
        skiff::Spacer(),
        bottomRow,
    }, 0).size(800, 480).bg(kBg).pad(12);
}

Element subPage(State<std::string>& page) {
    return skiff::VStack({
        skiff::Text(page.get()).ttf(kFont, 32).fg(kHi),
        skiff::Text("功能演示界面").ttf(kFont, 20).fg(kLo),
        skiff::Button("返回主页", [&page] { page.set("home"); })
            .size(220, 64).bg(kTile).ttf(kFont, 20).fg(kHi),
    }, 18).size(800, 480).bg(kBg).centered();
}

} // namespace

int main() {
    lv_init();
    skiff::lvgl::createSdl3Display(800, 480, "skiff - PND home");

    State<std::string> page("home");

    skiff::lvgl::LvglBackend backend(lv_scr_act());
    skiff::App app(backend, [&page]() -> Element {
        return page.get() == "home" ? homePage(page) : subPage(page);
    });
    app.bind(page);
    app.start();

    while (skiff::lvgl::sdl3Pump()) {
        lv_timer_handler();
        app.update();
    }

    skiff::lvgl::destroySdl3Display();
    return 0;
}
