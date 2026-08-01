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
        .size(w, h).bg(color).centered();
}

Element smallTile(const char* icon, const char* label, int w, int h,
                  State<std::string>& page) {
    const std::string name = label;
    return skiff::Button({
            skiff::Text(icon).font(24).fg(kHi),
            skiff::Text(label).ttf(kFont, 18).fg(kHi),
        }, [&page, name] { page.set(name); })
        .size(w, h).bg(kTile).centered();
}

Element statusBar() {
    return skiff::HStack({
        skiff::Text(clockText()).font(18).fg(kHi),
        skiff::Spacer(),
        skiff::Text(ICON_GPS).font(16).fg(kLo),
        skiff::Text(ICON_BT).font(16).fg(kLo),
        skiff::Text(ICON_WIFI).font(16).fg(kLo),
        skiff::Text(ICON_BATT).font(16).fg(kLo),
    }, 12).size(776, 24).centered();
}

// ---- 主页:磁贴入口 ----
Element homeBody(skiff::components::StateView& st) {
    State<std::string>& page = st.get<std::string>("page");
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
    }, 0).size(800, 480).bg(kBg).pad(12).padTop(0);
}

Element subPage(State<std::string>& page) {
    return skiff::VStack({
        skiff::Text(page.get()).ttf(kFont, 32).fg(kHi),
        skiff::Text("功能演示界面").ttf(kFont, 20).fg(kLo),
        skiff::Button("返回主页", [&page] { page.set("home"); })
            .size(220, 64).bg(kTile).ttf(kFont, 20).fg(kHi),
    }, 18).size(800, 480).bg(kBg).centered();
}

// ---- 设置页:各子菜单内容(供 SettingsPage 使用) ----
Element settingsRow(const char* label, const char* value) {
    return skiff::HStack({
        skiff::Text(label).ttf(kFont, 18).fg(kHi),
        skiff::Spacer(),
        skiff::Text(value).ttf(kFont, 16).fg(kLo),
    }, 0).size(560, 48).centered();
}

Element brightnessRow(State<int>& brightness) {
    return skiff::HStack({
        skiff::Text("亮度").ttf(kFont, 18).fg(kHi),
        skiff::Spacer(),
        skiff::Slider(brightness.get(), 0, 100,
                      [&brightness](int v) { brightness.set(v); })
            .size(180, 24),
        skiff::Text(std::to_string(brightness.get()) + "%")
            .ttf(kFont, 16).fg(kLo).size(48, 24),
    }, 12).size(560, 48).centered();
}

Element networkSubmenu() {
    return skiff::VStack({
        settingsRow("Wi-Fi", "已连接"),
        settingsRow("蓝牙", "开启"),
        settingsRow("移动数据", "关闭"),
        settingsRow("飞行模式", "关闭"),
        settingsRow("热点", "未开启"),
    }, 0).size(560, 432).pad(20);
}

Element displaySubmenu(State<int>& brightness) {
    return skiff::VStack({
        brightnessRow(brightness),
        settingsRow("自动调节", "开启"),
        settingsRow("夜间模式", "关闭"),
        settingsRow("分辨率", "800x480"),
        settingsRow("主题", "深色"),
    }, 0).size(560, 432).pad(20);
}

Element soundSubmenu() {
    return skiff::VStack({
        settingsRow("媒体音量", "60%"),
        settingsRow("导航音量", "80%"),
        settingsRow("提示音", "开启"),
        settingsRow("均衡器", "流行"),
    }, 0).size(560, 432).pad(20);
}

Element systemSubmenu() {
    return skiff::VStack({
        settingsRow("系统版本", "v1.2.0"),
        settingsRow("存储空间", "12GB/32GB"),
        settingsRow("语言", "简体中文"),
        settingsRow("恢复出厂", "-"),
        settingsRow("关于", "-"),
    }, 0).size(560, 432).pad(20);
}

// ---- 设置页:TopNav + TabView ----
Element settingsBody(skiff::components::StateView& st) {
    State<std::string>& page = st.get<std::string>("page");
    State<int>& tab = st.get<int>("tab");
    State<int>& brightness = st.get<int>("brightness");

    // 顶部导航条:返回按钮 + 标题
    skiff::components::TopNavOptions navOpt;
    navOpt.width = 800;
    navOpt.ttfPath = kFont;
    navOpt.bgColor = 0x1A222B;
    navOpt.hasBg = true;
    navOpt.leading = skiff::Button("返回主页", [&page] { page.set("home"); })
        .size(140, 36).bg(kTile).ttf(kFont, 16).fg(kHi);
    navOpt.hasLeading = true;

    skiff::components::TabViewOptions opt;
    opt.width = 800;
    opt.height = 480 - 48;
    opt.ttfPath = kFont;
    opt.activeBg = kNavi;
    opt.inactiveBg = kTile;
    opt.contentBg = kTile;
    opt.hasContentBg = true;
    return skiff::VStack({
        skiff::components::TopNav("设置", navOpt),
        skiff::components::TabView({
            {"网络", networkSubmenu()},
            {"显示", displaySubmenu(brightness)},
            {"声音", soundSubmenu()},
            {"系统", systemSubmenu()},
        }, tab, opt),
    }, 0).size(800, 480).bg(kBg);
}

} // namespace

int main() {
    lv_init();
    skiff::lvgl::createSdl3Display(800, 480, "skiff - PND home");

    // 路由:按名字注册页面,nav() 为全局导航状态(页面内 nav.set("名") 跳转)
    skiff::components::Router router("home");
    router.add("home", {
        skiff::components::stateRef("page", router.nav()),
    }, homeBody);
    router.add("设置", {
        skiff::components::stateRef("page", router.nav()),
        skiff::components::state<int>("tab", 0),         // 侧栏当前选中项
        skiff::components::state<int>("brightness", 80), // 亮度滑条
    }, settingsBody);
    // 未注册的路由(导航/音乐/...)走兜底演示页
    router.fallback([&router]() -> Element { return subPage(router.nav()); });

    skiff::lvgl::LvglBackend backend(lv_scr_act());
    skiff::App app(backend, [&router]() -> Element { return router.render(); });
    router.bind(app);
    app.start();

    while (skiff::lvgl::sdl3Pump()) {
        lv_timer_handler();
        app.update();
    }

    skiff::lvgl::destroySdl3Display();
    return 0;
}
