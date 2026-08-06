// pnd_sdl:车机 PND 风格主页 —— DSL 编写,平台无关。
//
// 本文件只描述 UI,不感知具体后端(LVGL/SDL3)或运行平台。
// 平台相关入口(如 macOS)在 platforms/mac/mac_platform.cpp 中实现。
// 文案经 pnd_i18n(业务) + skiff::i18n(框架);路由使用稳定英文 ID。
#include <cstdlib>
#include <ctime>
#include <string>

#include "skiff/skiff.hpp"
#include "pnd_i18n.hpp"

using skiff::Element;
using skiff::ElementView;
using skiff::State;
using namespace pnd::i18n;  // Key 枚举,供 tr(nav_home) 等使用

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

// routeId:稳定路由键; label:i18n 枚举
Element bigTile(const char* icon, const char* routeId, pnd::i18n::Key label,
                int w, int h, uint32_t color, skiff::components::Router& router) {
    const std::string id = routeId;
    return skiff::Button({
            skiff::Text(icon).font(32).fg(kHi),
            skiff::Text(skiff::i18n::t(label)).ttf(kFont, 28).fg(kHi),
        }, [&router, id] { router.push(id); })
        .size(w, h).bg(color).centered();
}

Element smallTile(const char* icon, const char* routeId, pnd::i18n::Key label,
                  int w, int h, skiff::components::Router& router) {
    const std::string id = routeId;
    return skiff::Button({
            skiff::Text(icon).font(24).fg(kHi),
            skiff::Text(skiff::i18n::t(label)).ttf(kFont, 18).fg(kHi),
        }, [&router, id] { router.push(id); })
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
                {tr(app_wifi), [&router, &menuExpanded] {
                menuExpanded.set(false);
                router.push("wifi");
                }},
                {tr(app_bluetooth), [&router, &menuExpanded] {
                    menuExpanded.set(false);
                    router.push("bluetooth");
                }},
                {tr(app_settings), [&router, &menuExpanded] {
                    menuExpanded.set(false);
                    router.push("settings");
                }},
            },
            {
                {tr(app_brightness), 0, 100, brightness,
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
        skiff::Text(pnd::i18n::routeTitle(router.current())).ttf(kFont, 32).fg(kHi),
        skiff::Text(tr(common_demo)).ttf(kFont, 20).fg(kLo),
        skiff::Button(tr(nav_back_page), [&router] { router.pop(); })
            .size(220, 64).bg(kTile).ttf(kFont, 20).fg(kHi),
    }, 18).size(800, 480).bg(kBg).centered();
}

Element settingsRow(const std::string& label, const std::string& value) {
    return skiff::HStack({
        skiff::Text(label).ttf(kFont, 18).fg(kHi),
        skiff::Spacer(),
        skiff::Text(value).ttf(kFont, 16).fg(kLo),
    }, 0).size(560, 48).centered();
}

Element languageRow(State<std::string>& locale) {
    const bool isEn = locale.get() == "en";
    const std::string next = isEn ? "zh-CN" : "en";
    const std::string shown = isEn ? tr(settings_lang_en) : tr(settings_lang_zh);
    return skiff::HStack({
        skiff::Text(tr(settings_language)).ttf(kFont, 18).fg(kHi),
        skiff::Spacer(),
        skiff::Button(shown, [&locale, next] {
            skiff::i18n::setLocale(next);
            locale.set(next);
        }).size(160, 36).bg(kNavi).ttf(kFont, 16).fg(kHi),
    }, 0).size(560, 48).centered();
}

Element brightnessRow(State<int>& brightness) {
    return skiff::HStack({
        skiff::Text(tr(app_brightness)).ttf(kFont, 18).fg(kHi),
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
        settingsRow(tr(settings_wifi), tr(common_connected)),
        settingsRow(tr(settings_bluetooth), tr(common_on)),
        settingsRow(tr(settings_mobile_data), tr(common_off)),
        settingsRow(tr(settings_airplane), tr(common_off)),
        settingsRow(tr(settings_hotspot), tr(common_not_enabled)),
    }, 0).size(560, 432).pad(20).bg(kTile);
}

Element displaySubmenu(State<int>& brightness) {
    return skiff::VStack({
        brightnessRow(brightness),
        settingsRow(tr(settings_auto_brightness), tr(common_on)),
        settingsRow(tr(settings_night_mode), tr(common_off)),
        settingsRow(tr(settings_resolution), "800x480"),
        settingsRow(tr(settings_theme), tr(settings_theme_dark)),
    }, 0).size(560, 432).pad(20).bg(kTile);
}

Element soundSubmenu() {
    return skiff::VStack({
        settingsRow(tr(settings_media_volume), "60%"),
        settingsRow(tr(settings_navi_volume), "80%"),
        settingsRow(tr(settings_beep), tr(common_on)),
        settingsRow(tr(settings_eq), tr(settings_eq_pop)),
    }, 0).size(560, 432).pad(20).bg(kTile);
}

Element systemSubmenu(State<std::string>& locale) {
    return skiff::VStack({
        settingsRow(tr(settings_version), "v1.2.0"),
        settingsRow(tr(settings_storage), "12GB/32GB"),
        languageRow(locale),
        settingsRow(tr(settings_reset), tr(common_dash)),
        settingsRow(tr(settings_about), tr(common_dash)),
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
        pnd::i18n::init("zh-CN");
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
            {"locale", "zh-CN"},
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
                skiff::Text(playing ? tr(music_playing) : tr(music_stopped))
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
                    skiff::components::TopNavView::routerHome(router(), tr(nav_home)).ttf(kFont, 16),
                    skiff::components::TopNavView::routerPrev(router(), tr(nav_back)).ttf(kFont, 16),
                })
                .title(skiff::Text(tr(app_music)).ttf(kFont, 20))
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
                     router().push("music");
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
                    skiff::components::TopNavView::routerHome(router(), tr(nav_home)).ttf(kFont, 16),
                    skiff::components::TopNavView::routerPrev(router(), tr(nav_back)).ttf(kFont, 16),
                })
                .title(skiff::Text(tr(app_media)).ttf(kFont, 20))
                .size(800, 48)
                .bg(0x1A222B),
                skiff::components::TabView({
                    {tr(media_video),   makeList(videoItems)},
                    {tr(media_music),   makeList(musicItems)},
                    {tr(media_image),   makeList(imageItems)},
                    {tr(media_ebook), makeList(ebookItems)},
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
                bigTile(ICON_GPS, "navi", pnd::i18n::app_navi, 510, 300, kNavi, router()),
                skiff::VStack({
                    bigTile(ICON_PLAY, "music", pnd::i18n::app_music, 256, 145, kMusic, router()),
                    bigTile(ICON_CALL, "phone", pnd::i18n::app_phone, 256, 145, kPhone, router()),
                }, 10),
            }, 10);

            Element bottomRow = skiff::HStack({
                smallTile(ICON_AUDIO, "radio", pnd::i18n::app_radio, 188, 86, router()),
                smallTile(ICON_VIDEO, "media", pnd::i18n::app_media, 188, 86, router()),
                smallTile(ICON_SETUP, "settings", pnd::i18n::app_settings, 188, 86, router()),
                smallTile(ICON_APPS, "apps", pnd::i18n::app_apps, 188, 86, router()),
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
            State<std::string>& locale_ = states().get<std::string>("locale");

            return skiff::VStack({
                skiff::components::TopNav({
                    skiff::components::TopNavView::routerHome(router(), tr(nav_home)).ttf(kFont, 16),
                    skiff::components::TopNavView::routerPrev(router(), tr(nav_back)).ttf(kFont, 16),
                })
                .title(skiff::Text(tr(app_settings)).ttf(kFont, 20))
                .size(800, 48)
                .bg(0x1A222B),
                skiff::components::TabView({
                    {tr(settings_network), networkSubmenu()},
                    {tr(settings_display), displaySubmenu(brightness_)},
                    {tr(settings_sound), soundSubmenu()},
                    {tr(settings_system), systemSubmenu(locale_)},
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
                {ICON_GPS,  tr(app_navi),   [this] { router().push("navi"); }},
                {ICON_PLAY, tr(app_music),   [this] { router().push("music"); }},
                {ICON_CALL, tr(app_phone_short),   [this] { router().push("phone"); }},
                {ICON_AUDIO,tr(app_radio), [this] { router().push("radio"); }},
                {ICON_VIDEO,tr(app_media), [this] { router().push("media"); }},
                {ICON_IMAGE,tr(app_gallery),   [this] { router().push("gallery"); }},
                {ICON_SETUP,tr(app_settings),   [this] { router().push("settings"); }},
                {ICON_WIFI, tr(app_wifi),  [this] { router().push("wifi"); }},
                {ICON_BT,   tr(app_bluetooth),   [this] { router().push("bluetooth"); }},
                {ICON_BATT, tr(app_battery),   [this] { router().push("battery"); }},
                {ICON_GPS,  tr(app_map),   [this] { router().push("map"); }},
                {ICON_CALL, tr(app_contacts), [this] { router().push("contacts"); }},
                {ICON_PLAY, tr(app_video),   [this] { router().push("video"); }},
                {ICON_IMAGE,tr(app_camera),   [this] { router().push("camera"); }},
                {ICON_AUDIO,tr(app_recorder),   [this] { router().push("recorder"); }},
                {ICON_SETUP,tr(app_calendar),   [this] { router().push("calendar"); }},
                {ICON_WIFI, tr(app_weather),   [this] { router().push("weather"); }},
                {ICON_BATT, tr(app_calculator), [this] { router().push("calculator"); }},
                {ICON_APPS, tr(app_files),   [this] { router().push("files"); }},
                {ICON_BT,   tr(app_clock),   [this] { router().push("clock"); }},
            };

            return skiff::VStack({
                skiff::components::TopNav({
                    skiff::components::TopNavView::routerHome(router(), tr(nav_home)).ttf(kFont, 16),
                    skiff::components::TopNavView::routerPrev(router(), tr(nav_back)).ttf(kFont, 16),
                })
                .title(skiff::Text(tr(app_apps)).ttf(kFont, 20))
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
        router().add("music", {}, musicBody);
        router().add("media", {}, mediaBody);
        router().add("apps", {}, appGridBody);
        router().add("settings", {
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
