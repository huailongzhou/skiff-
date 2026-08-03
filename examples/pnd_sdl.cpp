// pnd_sdl:车机 PND 风格主页 —— DSL 编写,平台无关。
//
// 本文件只描述 UI,不感知具体后端(LVGL/SDL3)或运行平台。
// 平台相关入口(如 macOS)在 platforms/mac/mac_platform.cpp 中实现。
#include <cstdlib>
#include <ctime>
#include <string>

#include "skiff/skiff.hpp"

using skiff::Element;
using skiff::ElementView;
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
const char* const ICON_APPS  = "\xEF\x80\x89";  // FontAwesome th-large
const char* const ICON_PREV  = "\xEF\x81\x88";  // FontAwesome step-backward
const char* const ICON_PAUSE = "\xEF\x81\x8C";  // FontAwesome pause
const char* const ICON_NEXT  = "\xEF\x81\x91";  // FontAwesome step-forward

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
                uint32_t color, skiff::components::Router& router) {
    const std::string name = label;
    return skiff::Button({
            skiff::Text(icon).font(32).fg(kHi),
            skiff::Text(label).ttf(kFont, 28).fg(kHi),
        }, [&router, name] { router.push(name); })
        .size(w, h).bg(color).centered();
}

Element smallTile(const char* icon, const char* label, int w, int h,
                  skiff::components::Router& router) {
    const std::string name = label;
    return skiff::Button({
            skiff::Text(icon).font(24).fg(kHi),
            skiff::Text(label).ttf(kFont, 18).fg(kHi),
        }, [&router, name] { router.push(name); })
        .size(w, h).bg(kTile).centered();
}

Element topMenuOverlay(State<bool>& menuExpanded, State<int>& brightness,
                       skiff::components::Router& router,
                       skiff::Platform& platform) {
    const int menuItemH = 64;
    return skiff::components::DropDown(menuExpanded)
        .layout(skiff::layout::hstack({
            skiff::layout::grid(2, 2).sizePct(30),
            skiff::layout::grid(2, 1).sizePct(70),
        }))
        .adapter({
            {
                {"Wi-Fi", [&router, &menuExpanded] {
                menuExpanded.set(false);
                router.push("Wi-Fi");
                }},
                {"蓝牙", [&router, &menuExpanded] {
                    menuExpanded.set(false);
                    router.push("蓝牙");
                }},
                {"设置", [&router, &menuExpanded] {
                    menuExpanded.set(false);
                    router.push("设置");
                }},
            },
            {
                {"亮度", 0, 100, brightness,
                 [&platform, &brightness](int v) {
                     brightness.set(v);
                     platform.invokeExternal("setBrightness",
                                            {std::to_string(v)});
                 }},
            }
        })
        .size(480, 0)
        .itemHeight(menuItemH)
        .bg(kTile)
        .ttf(kFont, 16)
        .fg(kHi)
        .alignRight()
        .slideInDown()
        .floating();
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

Element subPage(skiff::components::Router& router) {
    return skiff::VStack({
        skiff::Text(router.current()).ttf(kFont, 32).fg(kHi),
        skiff::Text("功能演示界面").ttf(kFont, 20).fg(kLo),
        skiff::Button("返回上一页", [&router] { router.pop(); })
            .size(220, 64).bg(kTile).ttf(kFont, 20).fg(kHi),
    }, 18).size(800, 480).bg(kBg).centered();
}

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
    }, 0).size(560, 432).pad(20).bg(kTile);
}

Element displaySubmenu(State<int>& brightness) {
    return skiff::VStack({
        brightnessRow(brightness),
        settingsRow("自动调节", "开启"),
        settingsRow("夜间模式", "关闭"),
        settingsRow("分辨率", "800x480"),
        settingsRow("主题", "深色"),
    }, 0).size(560, 432).pad(20).bg(kTile);
}

Element soundSubmenu() {
    return skiff::VStack({
        settingsRow("媒体音量", "60%"),
        settingsRow("导航音量", "80%"),
        settingsRow("提示音", "开启"),
        settingsRow("均衡器", "流行"),
    }, 0).size(560, 432).pad(20).bg(kTile);
}

Element systemSubmenu() {
    return skiff::VStack({
        settingsRow("系统版本", "v1.2.0"),
        settingsRow("存储空间", "12GB/32GB"),
        settingsRow("语言", "简体中文"),
        settingsRow("恢复出厂", "-"),
        settingsRow("关于", "-"),
    }, 0).size(560, 432).pad(20).bg(kTile);
}

} // namespace

namespace skiff {
namespace demo {

// PND 风格主页的 UI:继承 AppUi 获得 platform/router/states 能力,
// 这里只写 PND 特有的部分(能力声明、事件订阅、页面与下拉菜单)。
class PndUi : public components::AppUi {
public:
    explicit PndUi(Platform& platform)
        : components::AppUi(platform, "home") {
        // 注册全局状态(由 StateView 持有,bindAll 一次性绑定)
        globalStatesInit(skiff::components::state::BOOL, {
            {"menuExpanded", false},
            {"musicPlaying", false},
        });
        globalStatesInit(skiff::components::state::INT, {
            {"brightness", 80},
            {"musicProgress", 0},
            {"mediaCategory", 0},
        });
        globalStatesInit(skiff::components::state::STRING, {
            {"currentTrack", "assets/music/sample.wav"},
        });
        // 声明需要的平台能力,具体实现由平台入口注册
        platform.declare("setBrightness");
        platform.declare("playMusic");
        platform.declare("stopMusic");
        platform.declare("openFile");
        // 订阅平台事件(平台 → UI 上报,如播放进度)
        platform.on("musicProgress", [this](const std::vector<std::string>& args) {
            if (!args.empty()) {
                states().get<int>("musicProgress").set(std::atoi(args[0].c_str()));
            }
        });
        platform.on("musicEnded", [this](const std::vector<std::string>&) {
            states().get<bool>("musicPlaying").set(false);
            states().get<int>("musicProgress").set(0);
        });
        setupPages_();
        setupOverlay_(platform);
    }

    void toggleMenu() {
        State<bool>& e = states().get<bool>("menuExpanded");
        e.set(!e.get());
    }

private:
    void setupPages_() {
        auto musicBody = [this](components::StateView&) -> Element {
            State<std::string>& currentTrack_ = states().get<std::string>("currentTrack");
            const std::string track = currentTrack_.get();
            State<bool>& musicPlaying_ = states().get<bool>("musicPlaying");
            State<int>& musicProgress_ = states().get<int>("musicProgress");
            const bool playing = musicPlaying_.get();

            // 封面
            Element album = skiff::VStack({
                skiff::Text(ICON_AUDIO).font(56).fg(kHi),
            }).size(150, 150).bg(kMusic).centered();

            // 曲目信息 + 播放状态
            Element trackInfo = skiff::VStack({
                skiff::Text("sample").ttf(kFont, 22).fg(kHi),
                skiff::Text(track).ttf(kFont, 12).fg(kLo),
                skiff::Text(playing ? "正在播放" : "已停止")
                    .ttf(kFont, 14).fg(playing ? kMusic : kLo),
            }, 4).centered();

            // 控制按钮:快退 / 播放暂停 / 快进(快退快进演示进度 ±10)
            Element controls = skiff::HStack({
                skiff::Button({skiff::Text(ICON_PREV).font(24).fg(kLo)},
                              [this, &musicProgress_] { const int v = musicProgress_.get();
                                       musicProgress_.set(v > 10 ? v - 10 : 0); })
                    .size(56, 56).bg(kTile),
                skiff::Button({skiff::Text(playing ? ICON_PAUSE : ICON_PLAY)
                                   .font(28).fg(kHi)},
                              [this, track, &musicPlaying_] {
                                  if (musicPlaying_.get()) {
                                      platform().invokeExternal("stopMusic", {});
                                      musicPlaying_.set(false);
                                  } else {
                                      platform().invokeExternal("playMusic", {track});
                                      musicPlaying_.set(true);
                                  }
                              })
                    .size(80, 80).bg(kMusic),
                skiff::Button({skiff::Text(ICON_NEXT).font(24).fg(kLo)},
                              [this, &musicProgress_] { const int v = musicProgress_.get();
                                       musicProgress_.set(v < 90 ? v + 10 : 100); })
                    .size(56, 56).bg(kTile),
            }, 32).centered();

            // 进度条 + 百分比
            Element progressRow = skiff::HStack({
                skiff::Slider(musicProgress_.get(), 0, 100,
                              [this, &musicProgress_](int v) { musicProgress_.set(v); })
                    .size(440, 20),
                skiff::Text(std::to_string(musicProgress_.get()) + "%")
                    .ttf(kFont, 14).fg(kLo).size(40, 20),
            }, 12).centered();

            return skiff::VStack({
                skiff::components::TopNav({
                    skiff::components::TopNavView::routerHome(router()).ttf(kFont, 16),
                    skiff::components::TopNavView::routerPrev(router()).ttf(kFont, 16),
                })
                .title(skiff::Text("音乐").ttf(kFont, 20))
                .size(800, 48)
                .bg(0x1A222B),
                skiff::Spacer(),
                album,
                skiff::Spacer(),
                trackInfo,
                skiff::Spacer(),
                controls,
                skiff::Spacer(),
                progressRow,
                skiff::Spacer(),
            }, 0).size(800, 480).bg(kBg).centered();
        };

        auto mediaBody = [this](components::StateView&) -> Element {
            State<int>& category_ = states().get<int>("mediaCategory");

            auto makeList = [this](const std::vector<skiff::components::ListItem>& items) -> Element {
                return skiff::components::List(items)
                    .itemHeight(64)
                    .rowBg(0x26303B)   // 列表项背景
                    .subFg(kLo)
                    .as<ElementView>()
                    .ttf(kFont, 16)
                    .bg(0x1A222B)      // List 容器背景
                    .fg(kHi)
                    .size(800, 384);
            };

            std::vector<skiff::components::ListItem> videoItems = {
                {"sample video",
                 [this] { platform().invokeExternal("openFile", {"assets/media/sample.mp4"}); }},
            };
            std::vector<skiff::components::ListItem> musicItems = {
                {"Moment of Peace",
                 [this] {
                     const std::string track =
                         "assets/music/mickeyscat-moment-of-peace-mickeyscat-554494.mp3";
                     states().get<std::string>("currentTrack").set(track);
                     states().get<int>("musicProgress").set(0);
                     platform().invokeExternal("stopMusic", {});
                     platform().invokeExternal("playMusic", {track});
                     states().get<bool>("musicPlaying").set(true);
                     router().push("音乐");
                 }},
            };
            std::vector<skiff::components::ListItem> imageItems = {
                {"sample image",
                 [this] { platform().invokeExternal("openFile", {"assets/media/sample.jpg"}); }},
            };
            std::vector<skiff::components::ListItem> ebookItems = {
                {"sample ebook",
                 [this] { platform().invokeExternal("openFile", {"assets/media/sample.txt"}); }},
            };

            return skiff::VStack({
                skiff::components::TopNav({
                    skiff::components::TopNavView::routerHome(router()).ttf(kFont, 16),
                    skiff::components::TopNavView::routerPrev(router()).ttf(kFont, 16),
                })
                .title(skiff::Text("多媒体").ttf(kFont, 20))
                .size(800, 48)
                .bg(0x1A222B),
                skiff::components::TabView({
                    {"视频",   makeList(videoItems)},
                    {"音乐",   makeList(musicItems)},
                    {"图片",   makeList(imageItems)},
                    {"电子书", makeList(ebookItems)},
                }, category_)
                    .as<skiff::components::TabViewView>()
                    .applyBgOption({
                        {skiff::components::tabview::first(), skiff::elements::state::selected(),   0x26303B},
                        {skiff::components::tabview::first(), skiff::elements::state::unselected(), 0x1A222B},
                        {skiff::components::tabview::first(), skiff::elements::state::pressed(),    0x1565D8},
                        {skiff::components::tabview::content(), skiff::elements::state(),           kTile},
                    })
                    .ttf(kFont, 18)
                    .size(800, 480 - 48),
            }, 0).size(800, 480).bg(kBg);
        };

        auto homeBody = [this](components::StateView&) -> Element {
            Element topBar = statusBar();

            Element mainRow = skiff::HStack({
                bigTile(ICON_GPS, "导航", 510, 300, kNavi, router()),
                skiff::VStack({
                    bigTile(ICON_PLAY, "音乐", 256, 145, kMusic, router()),
                    bigTile(ICON_CALL, "蓝牙电话", 256, 145, kPhone, router()),
                }, 10),
            }, 10);

            Element bottomRow = skiff::HStack({
                smallTile(ICON_AUDIO, "收音机", 188, 86, router()),
                smallTile(ICON_VIDEO, "多媒体", 188, 86, router()),
                smallTile(ICON_SETUP, "设置",   188, 86, router()),
                smallTile(ICON_APPS,  "应用",   188, 86, router()),
            }, 8);

            return skiff::VStack({
                topBar,
                skiff::Spacer(),
                mainRow,
                skiff::Spacer(),
                bottomRow,
            }, 0).size(800, 480).bg(kBg).pad(12).padTop(0);
        };

        auto settingsBody = [this](components::StateView& st) -> Element {
            State<int>& tab = st.get<int>("tab");
            State<int>& brightness_ = states().get<int>("brightness");

            return skiff::VStack({
                skiff::components::TopNav({
                    skiff::components::TopNavView::routerHome(router()).ttf(kFont, 16),
                    skiff::components::TopNavView::routerPrev(router()).ttf(kFont, 16),
                })
                .size(800, 48)
                .bg(0x1A222B),
                skiff::components::TabView({
                    {"网络", networkSubmenu()},
                    {"显示", displaySubmenu(brightness_)},
                    {"声音", soundSubmenu()},
                    {"系统", systemSubmenu()},
                }, tab)
                    .as<skiff::components::TabViewView>()
                    .applyBgOption({
                        {skiff::components::tabview::first(), skiff::elements::state::selected(), kNavi},
                        {skiff::components::tabview::first(), skiff::elements::state::unselected(), kTile},
                        {skiff::components::tabview::first(), skiff::elements::state::pressed(), 0x2E7D32},
                        {skiff::components::tabview::content(), skiff::elements::state(), 0x000000},
                    })
                    .ttf(kFont, 18)
                    .size(800, 480 - 48),
            }, 0).size(800, 480).bg(kBg);
        };

        auto appGridBody = [this](components::StateView&) -> Element {
            using skiff::components::AppIcon;
            using skiff::components::AppGrid;

            std::vector<AppIcon> apps = {
                {ICON_GPS,  "导航",   [this] { router().push("导航"); }},
                {ICON_PLAY, "音乐",   [this] { router().push("音乐"); }},
                {ICON_CALL, "电话",   [this] { router().push("电话"); }},
                {ICON_AUDIO,"收音机", [this] { router().push("收音机"); }},
                {ICON_VIDEO,"多媒体", [this] { router().push("多媒体"); }},
                {ICON_IMAGE,"相册",   [this] { router().push("相册"); }},
                {ICON_SETUP,"设置",   [this] { router().push("设置"); }},
                {ICON_WIFI, "Wi-Fi",  [this] { router().push("Wi-Fi"); }},
                {ICON_BT,   "蓝牙",   [this] { router().push("蓝牙"); }},
                {ICON_BATT, "电量",   [this] { router().push("电量"); }},
                {ICON_GPS,  "地图",   [this] { router().push("地图"); }},
                {ICON_CALL, "通讯录", [this] { router().push("通讯录"); }},
                {ICON_PLAY, "视频",   [this] { router().push("视频"); }},
                {ICON_IMAGE,"相机",   [this] { router().push("相机"); }},
                {ICON_AUDIO,"录音",   [this] { router().push("录音"); }},
                {ICON_SETUP,"日历",   [this] { router().push("日历"); }},
                {ICON_WIFI, "天气",   [this] { router().push("天气"); }},
                {ICON_BATT, "计算器", [this] { router().push("计算器"); }},
                {ICON_APPS, "文件",   [this] { router().push("文件"); }},
                {ICON_BT,   "时钟",   [this] { router().push("时钟"); }},
            };

            return skiff::VStack({
                skiff::components::TopNav({
                    skiff::components::TopNavView::routerHome(router()).ttf(kFont, 16),
                    skiff::components::TopNavView::routerPrev(router()).ttf(kFont, 16),
                })
                .title(skiff::Text("应用").ttf(kFont, 20))
                .size(800, 48)
                .bg(0x1A222B),
                AppGrid(apps)
                    .cols(2).rows(2)
                    .horizontal()
                    .pageSize(800, 432)
                    .ttf(kFont, 16)
                    .size(800, 432)
            }, 0).size(800, 480).bg(kBg);
        };

        router().add("home", {}, homeBody);
        router().add("音乐", {}, musicBody);
        router().add("多媒体", {}, mediaBody);
        router().add("应用", {}, appGridBody);
        router().add("设置", {
            skiff::components::state::of<int>("tab", 0),
        }, settingsBody);
        router().fallback([this]() -> Element { return subPage(router()); });
    }

    void setupOverlay_(Platform& platform) {
        router().setOverlayBuilder([this, &platform]() -> std::vector<Element> {
            State<bool>& menuExpanded_ = states().get<bool>("menuExpanded");
            State<int>& brightness_ = states().get<int>("brightness");
            if (!menuExpanded_.get()) return {};
            return {
                skiff::TapArea([this] {
                    states().get<bool>("menuExpanded").set(false);
                })
                    .sizePct(100, 100)
                    .floating(),
                topMenuOverlay(menuExpanded_, brightness_, router(), platform),
            };
        });
    }
};

} // namespace demo
} // namespace skiff
