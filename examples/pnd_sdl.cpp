// pnd_sdl:车机 PND 风格主页 —— DSL 编写,平台无关。
//
// 本文件只描述 UI,不感知具体后端(LVGL/SDL3)或运行平台。
// 平台相关入口(如 macOS)在 platforms/mac/mac_platform.cpp 中实现。
// 文案经 pnd_i18n(业务) + skiff::i18n(框架);路由使用稳定英文 ID。
#include <chrono>
#include <cstdio>
#include <ctime>
#include <functional>
#include <string>

#include "skiff/skiff.hpp"
#include "pnd_i18n.hpp"
#include "pnd_state.hpp"
#include "pnd_platform.hpp"
#include "app_core/music_scene.hpp"
#include "app_core/physics_scene.hpp"
#include "app_core/scene_host.hpp"
#include "physics_draw.hpp"

using skiff::Element;
using skiff::ElementView;
using skiff::State;
using namespace pnd::i18n;  // Key 枚举,供 SKIFF_TR(nav_home) 等使用

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
const char* const ICON_PREV    = "\xEF\x81\x88";  // FontAwesome step-backward
const char* const ICON_PAUSE   = "\xEF\x81\x8C";  // FontAwesome pause
const char* const ICON_NEXT    = "\xEF\x81\x91";  // FontAwesome step-forward
const char* const ICON_SHUFFLE = "\xEF\x81\xB4";  // FontAwesome random
const char* const ICON_LOOP    = "\xEF\x81\xB9";  // FontAwesome retweet / repeat
const char* const ICON_VOLUME  = "\xEF\x80\xA8";  // FontAwesome volume-up
const char* const ICON_LIST    = "\xEF\x80\x8A";  // FontAwesome list
const char* const ICON_PHYSICS = "\xEF\x81\xA7";  // FontAwesome plus(投放刚体)

// ---- 配色 ----
const uint32_t kBg    = 0x0F141A;  // 页面底色(深蓝黑)
const uint32_t kTile  = 0x26303B;  // 普通磁贴
const uint32_t kNavi  = 0x1565D8;  // 导航蓝
const uint32_t kMusic = 0xD84315;  // 音乐橙红
const uint32_t kPhone = 0x2E7D32;  // 电话绿
const uint32_t kPhysics = 0x6A1B9A;  // 物理紫
const uint32_t kHi    = 0xFFFFFF;
const uint32_t kLo    = 0x9AA4B0;

// 物理页画布:TopNav 48 + 右侧操作栏 220,画布占剩余区域
const int kPhysSideW = 220;
const int kPhysCanvasW = 800 - kPhysSideW;
const int kPhysCanvasH = 480 - 48;

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

Element physicsToolBtn(const std::string& label, std::function<void()> onTap) {
    return skiff::Button(label, std::move(onTap))
        .widthPct(100)
        .size(0, 40)
        .bg(kTile)
        .fg(kHi)
        .radius(6)
        .ttf(kFont, 16);
}

Element topMenuOverlay(State<bool>& menuExpanded, State<int>& brightness,
                       skiff::components::Router& router,
                       std::function<void(int)> onBrightness) {
    const int menuItemH = 64;
    return skiff::components::DropDown(menuExpanded)
        .layout(skiff::layout::hstack({
            skiff::layout::grid(2, 2).sizePct(30),
            skiff::layout::grid(2, 1).sizePct(70),
        }))
        .adapter({
            {
                {SKIFF_TR(app_wifi), [&router, &menuExpanded] {
                menuExpanded.set(false);
                router.push("wifi");
                }},
                {SKIFF_TR(app_bluetooth), [&router, &menuExpanded] {
                    menuExpanded.set(false);
                    router.push("bluetooth");
                }},
                {SKIFF_TR(app_settings), [&router, &menuExpanded] {
                    menuExpanded.set(false);
                    router.push("settings");
                }},
            },
            {
                {SKIFF_TR(app_brightness), 0, 100, brightness,
                 [&brightness, onBrightness](int v) {
                     brightness.set(v);
                     onBrightness(v);
                 }},
            }
        })
        .widthPct(60)
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
    }, 12).size(0, 24).widthPct(100).centered();
}

Element subPage(skiff::components::Router& router) {
    return skiff::VStack({
        skiff::Text(pnd::i18n::routeTitle(router.current())).ttf(kFont, 32).fg(kHi),
        skiff::Text(SKIFF_TR(common_demo)).ttf(kFont, 20).fg(kLo),
        skiff::Button(SKIFF_TR(nav_back_page), [&router] { router.pop(); })
            .size(220, 64).bg(kTile).ttf(kFont, 20).fg(kHi),
    }, 18).sizePct(100, 100).bg(kBg).centered();
}

Element settingsRow(const std::string& label, const std::string& value) {
    return skiff::HStack({
        skiff::Text(label).ttf(kFont, 18).fg(kHi),
        skiff::Spacer(),
        skiff::Text(value).ttf(kFont, 16).fg(kLo),
    }, 0).size(0, 48).widthPct(100).centered();
}

Element languageRow(const std::string& locale,
                    const std::function<void(const std::string&)>& onSelect) {
    const bool isEn = locale == "en";
    const std::string next = isEn ? "zh-CN" : "en";
    const std::string shown = isEn ? SKIFF_TR(settings_lang_en) : SKIFF_TR(settings_lang_zh);
    return skiff::HStack({
        skiff::Text(SKIFF_TR(settings_language)).ttf(kFont, 18).fg(kHi),
        skiff::Spacer(),
        skiff::Button(shown, [onSelect, next] { onSelect(next); })
            .size(160, 36).bg(kNavi).ttf(kFont, 16).fg(kHi),
    }, 0).size(0, 48).widthPct(100).centered();
}

Element brightnessRow(int brightness, std::function<void(int)> onChange) {
    return skiff::HStack({
        skiff::Text(SKIFF_TR(app_brightness)).ttf(kFont, 18).fg(kHi),
        skiff::Spacer(),
        skiff::Slider(brightness, 0, 100, std::move(onChange))
            .size(180, 24),
        skiff::Text(std::to_string(brightness) + "%")
            .ttf(kFont, 16).fg(kLo).size(48, 24),
    }, 12).size(0, 48).widthPct(100).centered();
}

// ---- 音乐播放:横屏 Now Playing(参考 CarPlay / Apple Music) ----

std::string formatClock(int sec) {
    if (sec < 0) sec = 0;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d:%02d", sec / 60, sec % 60);
    return buf;
}

Element musicAlbumArt(const app::TrackInfo& track) {
    return skiff::VStack({
        skiff::VStack({
            skiff::Text(ICON_AUDIO).font(72).fg(0xFFFFFF),
        }).size(220, 220).bg(track.artColor).radius(16).centered(),
    }).size(236, 236).bg(0x0A0E12).radius(20).centered();
}

Element musicTitleBlock(const app::TrackInfo& track) {
    const std::string artist = (track.artist && track.artist[0])
                                   ? std::string(track.artist)
                                   : std::string(SKIFF_TR(music_unknown_artist));
    return skiff::VStack({
        skiff::Text(SKIFF_TR(music_now_playing)).ttf(kFont, 13).fg(kMusic),
        skiff::Text(track.title).ttf(kFont, 26).fg(kHi),
        skiff::Text(artist).ttf(kFont, 16).fg(kLo),
    }, 4);
}

Element musicProgressRow(int progress, int durationSec,
                         std::function<void(int)> onSeek) {
    int elapsed = 0;
    if (durationSec > 0) elapsed = progress * durationSec / 100;
    if (elapsed > durationSec) elapsed = durationSec;
    return skiff::HStack({
        skiff::Text(formatClock(elapsed)).ttf(kFont, 12).fg(kLo).size(44, 16),
        skiff::Slider(progress, 0, 100, std::move(onSeek))
            .expand()
            .size(0, 16),
        skiff::Text(formatClock(durationSec)).ttf(kFont, 12).fg(kLo).size(44, 16),
    }, 10).widthPct(100).centered();
}

Element musicIconBtn(const char* icon, uint32_t fg, int size,
                     std::function<void()> onTap) {
    return skiff::Button({skiff::Text(icon).font(size / 2).fg(fg)},
                         std::move(onTap))
        .size(size, size)
        .bg(kTile)
        .radius(size / 2);
}

Element musicPlayButton(bool playing, std::function<void()> onTap) {
    return skiff::Button({skiff::Text(playing ? ICON_PAUSE : ICON_PLAY)
                              .font(28)
                              .fg(0x1A1A1A)},
                         std::move(onTap))
        .size(72, 72)
        .bg(0xFFFFFF)
        .radius(36);
}

Element networkSubmenu() {
    return skiff::VStack({
        settingsRow(SKIFF_TR(settings_wifi), SKIFF_TR(common_connected)),
        settingsRow(SKIFF_TR(settings_bluetooth), SKIFF_TR(common_on)),
        settingsRow(SKIFF_TR(settings_mobile_data), SKIFF_TR(common_off)),
        settingsRow(SKIFF_TR(settings_airplane), SKIFF_TR(common_off)),
        settingsRow(SKIFF_TR(settings_hotspot), SKIFF_TR(common_not_enabled)),
    }, 0).sizePct(100, 100).pad(20).bg(kTile);
}

Element displaySubmenu(int brightness, std::function<void(int)> onBrightness) {
    return skiff::VStack({
        brightnessRow(brightness, std::move(onBrightness)),
        settingsRow(SKIFF_TR(settings_auto_brightness), SKIFF_TR(common_on)),
        settingsRow(SKIFF_TR(settings_night_mode), SKIFF_TR(common_off)),
        settingsRow(SKIFF_TR(settings_resolution), "800x480"),
        settingsRow(SKIFF_TR(settings_theme), SKIFF_TR(settings_theme_dark)),
    }, 0).sizePct(100, 100).pad(20).bg(kTile);
}

Element soundSubmenu() {
    return skiff::VStack({
        settingsRow(SKIFF_TR(settings_media_volume), "60%"),
        settingsRow(SKIFF_TR(settings_navi_volume), "80%"),
        settingsRow(SKIFF_TR(settings_beep), SKIFF_TR(common_on)),
        settingsRow(SKIFF_TR(settings_eq), SKIFF_TR(settings_eq_pop)),
    }, 0).sizePct(100, 100).pad(20).bg(kTile);
}

Element systemSubmenu(const std::string& locale,
                      const std::function<void(const std::string&)>& onSelectLocale) {
    return skiff::VStack({
        settingsRow(SKIFF_TR(settings_version), "v1.2.0"),
        settingsRow(SKIFF_TR(settings_storage), "12GB/32GB"),
        languageRow(locale, onSelectLocale),
        settingsRow(SKIFF_TR(settings_reset), SKIFF_TR(common_dash)),
        settingsRow(SKIFF_TR(settings_about), SKIFF_TR(common_dash)),
    }, 0).sizePct(100, 100).pad(20).bg(kTile);
}

} // namespace

namespace skiff {
namespace demo {

// PND 风格主页的 UI:继承 AppUi 获得 platform/router/states 能力,
// 这里只写 PND 特有的部分(页面与下拉菜单)。
class PndUi : public components::AppUi {
public:
    explicit PndUi(Platform& platform)
        : components::AppUi(platform, "home"),
          musicSink_(platform),
          display_(platform),
          physics_(kPhysCanvasW, kPhysCanvasH),
          physicsPage_(0),
          musicPage_(0) {
        tickLast_ = std::chrono::steady_clock::now();
        music_.setSink(&musicSink_);
        pnd::bindPlatform(platform, music_);
        scenes_.add(music_);
        scenes_.add(physics_);
        scenes_.activate("music");  // 后台常驻,离页不停播
        pnd::i18n::init("zh-CN");
        // 注册全局状态(由 StateView 持有,bindAll 一次性绑定)
        globalStatesInit(skiff::components::state::BOOL, {
            {pnd::g::menuExpanded, false},
        });
        globalStatesInit(skiff::components::state::INT, {
            {pnd::g::brightness, 80},
            {pnd::g::mediaCategory, 0},
        });
        globalStatesInit(skiff::components::state::STRING, {
            {pnd::g::locale, "zh-CN"},
        });
        setupPages_();
        setupOverlay_();
        physics_.onChange([this] { syncPhysicsUi_(); });
        music_.onChange([this] { syncMusicUi_(); });
    }

    void toggleMenu() {
        State<bool>& e = states().get<bool>(pnd::g::menuExpanded);
        e.set(!e.get());
    }

    void tick() {
        const std::chrono::steady_clock::time_point now =
            std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - tickLast_).count();
        tickLast_ = now;
        if (dt < 0.0f) dt = 0.0f;
        if (dt > 0.25f) dt = 0.25f;

        if (router().current() == "physics") scenes_.activate("physics");
        else scenes_.deactivate("physics");
        scenes_.tick(dt);
    }

private:
    void applyLocale_(const std::string& next) {
        skiff::i18n::setLocale(next);
        states().get<std::string>(pnd::g::locale).set(next);
    }

    void setupPages_() {
        auto musicBody = [this](components::StateView& st) -> Element {
            State<int>& musicProgress_ = st.get<int>(pnd::music::progress);
            State<bool>& musicPlaying_ = st.get<bool>(pnd::music::playing);
            State<std::string>& currentTrack_ =
                st.get<std::string>(pnd::music::currentTrack);
            State<bool>& shuffle_ = st.get<bool>(pnd::music::shuffle);
            State<int>& repeat_ = st.get<int>(pnd::music::repeat);
            State<int>& volume_ = st.get<int>(pnd::music::volume);

            Element album = skiff::Watch(currentTrack_, [this](const std::string&) {
                return musicAlbumArt(music_.current());
            });

            Element titles = skiff::Watch(currentTrack_, [this](const std::string&) {
                return musicTitleBlock(music_.current());
            });

            Element progressRow = skiff::Watch(
                currentTrack_, [this, &musicProgress_](const std::string&) {
                    const int duration = music_.current().durationSec;
                    return skiff::Watch(
                        musicProgress_, [this, duration](int p) {
                            return musicProgressRow(p, duration, [this](int v) {
                                music_.seek(v);
                            });
                        });
                });

            Element shuffleBtn = skiff::Watch(shuffle_, [this](bool on) {
                return musicIconBtn(ICON_SHUFFLE, on ? kMusic : kLo, 48,
                                    [this] { music_.toggleShuffle(); });
            });

            Element prevBtn = musicIconBtn(
                ICON_PREV, kHi, 52, [this] { music_.prev(); });

            Element playBtn = skiff::Watch(
                musicPlaying_, [this](bool playing) {
                    return musicPlayButton(playing, [this] { music_.togglePlay(); });
                });

            Element nextBtn = musicIconBtn(
                ICON_NEXT, kHi, 52, [this] { music_.next(); });

            Element repeatBtn = skiff::Watch(repeat_, [this](int mode) {
                return musicIconBtn(ICON_LOOP, mode != 0 ? kMusic : kLo, 48,
                                    [this] { music_.toggleRepeat(); });
            });

            Element controls = skiff::HStack({
                shuffleBtn,
                prevBtn,
                playBtn,
                nextBtn,
                repeatBtn,
            }, 16).centered();

            Element volumeRow = skiff::Watch(volume_, [this](int v) {
                return skiff::HStack({
                    skiff::Text(ICON_VOLUME).font(16).fg(kLo),
                    skiff::Slider(v, 0, 100, [this](int n) { music_.setVolume(n); })
                        .expand()
                        .size(0, 16),
                    skiff::Text(std::to_string(v) + "%")
                        .ttf(kFont, 12).fg(kLo).size(40, 16),
                }, 10).widthPct(100).centered();
            });

            Element queueBtn = musicIconBtn(
                ICON_LIST, kLo, 40, [this] { router().push("media"); });

            Element right = skiff::VStack({
                skiff::HStack({
                    skiff::Spacer(),
                    queueBtn,
                }, 0).widthPct(100),
                titles,
                skiff::Spacer(),
                progressRow,
                controls,
                volumeRow,
                skiff::Spacer(),
            }, 12).expand().pad(16).padRight(24);

            return skiff::VStack({
                skiff::components::TopNav({
                    skiff::components::TopNavView::routerHome(router(), SKIFF_TR(nav_home)).ttf(kFont, 16),
                    skiff::components::TopNavView::routerPrev(router(), SKIFF_TR(nav_back)).ttf(kFont, 16),
                })
                .title(skiff::Text(SKIFF_TR(app_music)).ttf(kFont, 20))
                .widthPct(100)
                .bg(0x1A222B),
                skiff::HStack({
                    skiff::VStack({album}, 0).sizePct(42, 100).centered(),
                    right,
                }, 0).widthPct(100).expand(),
            }, 0).sizePct(100, 100).bg(kBg);
        };

        auto mediaBody = [this](components::StateView&) -> Element {
            State<int>& category_ = states().get<int>(pnd::g::mediaCategory);

            auto makeList = [this](const std::vector<skiff::components::ListItem>& items) -> Element {
                return skiff::components::List(items)
                    .itemHeight(64)
                    .rowBg(0x26303B)   // 列表项背景
                    .subFg(kLo)
                    .as<ElementView>()
                    .ttf(kFont, 16)
                    .bg(0x1A222B)      // List 容器背景
                    .fg(kHi)
                    .sizePct(100, 100);
            };

            std::vector<skiff::components::ListItem> videoItems = {
                {"sample video",
                 [this] { platform().invokeExternal("openFile", {"assets/media/sample.mp4"}); }},
            };
            std::vector<skiff::components::ListItem> musicItems = {
                {music_.track(1).title, music_.track(1).artist,
                 [this] {
                     music_.playTrack(music_.track(1).path);
                     router().push("music");
                 }},
                {music_.track(0).title, music_.track(0).artist,
                 [this] {
                     music_.playTrack(music_.track(0).path);
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
                    skiff::components::TopNavView::routerHome(router(), SKIFF_TR(nav_home)).ttf(kFont, 16),
                    skiff::components::TopNavView::routerPrev(router(), SKIFF_TR(nav_back)).ttf(kFont, 16),
                })
                .title(skiff::Text(SKIFF_TR(app_media)).ttf(kFont, 20))
                .widthPct(100)
                .bg(0x1A222B),
                skiff::components::TabView({
                    {SKIFF_TR(media_video),   makeList(videoItems)},
                    {SKIFF_TR(media_music),   makeList(musicItems)},
                    {SKIFF_TR(media_image),   makeList(imageItems)},
                    {SKIFF_TR(media_ebook), makeList(ebookItems)},
                }, category_)
                    .as<skiff::components::TabViewView>()
                    .applyBgOption({
                        {skiff::components::tabview::first(), skiff::elements::state::selected(),   0x26303B},
                        {skiff::components::tabview::first(), skiff::elements::state::unselected(), 0x1A222B},
                        {skiff::components::tabview::first(), skiff::elements::state::pressed(),    0x1565D8},
                        {skiff::components::tabview::content(), skiff::elements::state(),           kTile},
                    })
                    .ttf(kFont, 18)
                    .widthPct(100).expand(),
            }, 0).sizePct(100, 100).bg(kBg);
        };

        auto physicsBody = [this](components::StateView& st) -> Element {
            State<int>& frame = st.get<int>(pnd::phys::frame);
            State<bool>& paused = st.get<bool>(pnd::phys::paused);
            State<int>& shape = st.get<int>(pnd::phys::shape);

            Element canvas = skiff::Watch(frame, [this](int) -> Element {
                return skiff::Canvas(kPhysCanvasW, kPhysCanvasH,
                                     [this](skiff::CanvasContext& c) {
                                         pnd::physics::paintScene(c, physics_);
                                     })
                    .onTapAt([this](int x, int y) {
                        physics_.spawnAtCanvas(x, y);
                    });
            });

            Element side = skiff::VStack({
                skiff::Text(SKIFF_TR(physics_hint))
                    .ttf(kFont, 14)
                    .fg(kLo)
                    .widthPct(100),
                skiff::Watch(paused, [this](bool p) -> Element {
                    return physicsToolBtn(p ? SKIFF_TR(physics_resume)
                                            : SKIFF_TR(physics_pause),
                                          [this] { physics_.togglePaused(); });
                }),
                skiff::Watch(shape, [this](int s) -> Element {
                    return physicsToolBtn(s == 0 ? SKIFF_TR(physics_drop_box)
                                                 : SKIFF_TR(physics_drop_ball),
                                          [this] {
                                              physics_.setDropCircle(
                                                  !physics_.dropCircle());
                                          });
                }),
                physicsToolBtn(SKIFF_TR(physics_reset), [this] {
                    physics_.reset();
                }),
                skiff::Spacer(),
            }, 10).size(kPhysSideW, kPhysCanvasH).pad(12).bg(0x1A222B);

            return skiff::VStack({
                skiff::components::TopNav({
                    skiff::components::TopNavView::routerHome(router(), SKIFF_TR(nav_home)).ttf(kFont, 16),
                    skiff::components::TopNavView::routerPrev(router(), SKIFF_TR(nav_back)).ttf(kFont, 16),
                })
                .title(skiff::Text(SKIFF_TR(app_physics)).ttf(kFont, 20))
                .widthPct(100)
                .bg(0x1A222B),
                skiff::HStack({
                    canvas,
                    side,
                }, 0).widthPct(100).expand(),
            }, 0).sizePct(100, 100).bg(kBg);
        };

        auto gamesBody = [this](components::StateView&) -> Element {
            std::vector<skiff::components::ListItem> games = {
                {SKIFF_TR(app_physics), SKIFF_TR(game_physics_sub),
                 [this] { router().push("physics"); }},
            };
            return skiff::VStack({
                skiff::components::TopNav({
                    skiff::components::TopNavView::routerHome(router(), SKIFF_TR(nav_home)).ttf(kFont, 16),
                    skiff::components::TopNavView::routerPrev(router(), SKIFF_TR(nav_back)).ttf(kFont, 16),
                })
                .title(skiff::Text(SKIFF_TR(app_games)).ttf(kFont, 20))
                .widthPct(100)
                .bg(0x1A222B),
                skiff::components::List(games)
                    .itemHeight(72)
                    .rowBg(0x26303B)
                    .subFg(kLo)
                    .as<ElementView>()
                    .ttf(kFont, 18)
                    .bg(0x1A222B)
                    .fg(kHi)
                    .widthPct(100)
                    .expand(),
            }, 0).sizePct(100, 100).bg(kBg);
        };

        auto homeBody = [this](components::StateView&) -> Element {
            Element topBar = statusBar();

            Element mainRow = skiff::HStack({
                bigTile(ICON_GPS, "navi", pnd::i18n::app_navi, 510, 300, kNavi, router()),
                skiff::VStack({
                    bigTile(ICON_PHYSICS, "games", pnd::i18n::app_games, 256, 145, kPhysics, router()),
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
            }, 0).sizePct(100, 100).bg(kBg).pad(12).padTop(0);
        };

        auto settingsBody = [this](components::StateView& st) -> Element {
            State<int>& tab = st.get<int>(pnd::settings::tab);
            State<int>& brightness = states().get<int>(pnd::g::brightness);
            State<std::string>& locale = states().get<std::string>(pnd::g::locale);

            return skiff::VStack({
                skiff::components::TopNav({
                    skiff::components::TopNavView::routerHome(router(), SKIFF_TR(nav_home)).ttf(kFont, 16),
                    skiff::components::TopNavView::routerPrev(router(), SKIFF_TR(nav_back)).ttf(kFont, 16),
                })
                .title(skiff::Text(SKIFF_TR(app_settings)).ttf(kFont, 20))
                .widthPct(100)
                .bg(0x1A222B),
                skiff::components::TabView({
                    {SKIFF_TR(settings_network), networkSubmenu()},
                    {SKIFF_TR(settings_display), skiff::Watch(brightness, [this, &brightness](int v) {
                        return displaySubmenu(v, [this, &brightness](int n) {
                            brightness.set(n);
                            display_.setBrightness(n);
                        });
                    })},
                    {SKIFF_TR(settings_sound), soundSubmenu()},
                    {SKIFF_TR(settings_system), systemSubmenu(locale.get(), [this](const std::string& next) {
                        applyLocale_(next);
                    })},
                }, tab)
                    .as<skiff::components::TabViewView>()
                    .applyBgOption({
                        {skiff::components::tabview::first(), skiff::elements::state::selected(), kNavi},
                        {skiff::components::tabview::first(), skiff::elements::state::unselected(), kTile},
                        {skiff::components::tabview::first(), skiff::elements::state::pressed(), 0x2E7D32},
                        {skiff::components::tabview::content(), skiff::elements::state(), 0x000000},
                    })
                    .ttf(kFont, 18)
                    .widthPct(100).expand(),
            }, 0).sizePct(100, 100).bg(kBg);
        };

        auto appGridBody = [this](components::StateView&) -> Element {
            using skiff::components::AppIcon;
            using skiff::components::AppGrid;

            std::vector<AppIcon> apps = {
                {ICON_PHYSICS, SKIFF_TR(app_games), [this] { router().push("games"); }},
                {ICON_GPS,  SKIFF_TR(app_navi),   [this] { router().push("navi"); }},
                {ICON_PLAY, SKIFF_TR(app_music),   [this] { router().push("music"); }},
                {ICON_CALL, SKIFF_TR(app_phone_short),   [this] { router().push("phone"); }},
                {ICON_AUDIO,SKIFF_TR(app_radio), [this] { router().push("radio"); }},
                {ICON_VIDEO,SKIFF_TR(app_media), [this] { router().push("media"); }},
                {ICON_IMAGE,SKIFF_TR(app_gallery),   [this] { router().push("gallery"); }},
                {ICON_SETUP,SKIFF_TR(app_settings),   [this] { router().push("settings"); }},
                {ICON_WIFI, SKIFF_TR(app_wifi),  [this] { router().push("wifi"); }},
                {ICON_BT,   SKIFF_TR(app_bluetooth),   [this] { router().push("bluetooth"); }},
                {ICON_BATT, SKIFF_TR(app_battery),   [this] { router().push("battery"); }},
                {ICON_GPS,  SKIFF_TR(app_map),   [this] { router().push("map"); }},
                {ICON_CALL, SKIFF_TR(app_contacts), [this] { router().push("contacts"); }},
                {ICON_PLAY, SKIFF_TR(app_video),   [this] { router().push("video"); }},
                {ICON_IMAGE,SKIFF_TR(app_camera),   [this] { router().push("camera"); }},
                {ICON_AUDIO,SKIFF_TR(app_recorder),   [this] { router().push("recorder"); }},
                {ICON_SETUP,SKIFF_TR(app_calendar),   [this] { router().push("calendar"); }},
                {ICON_WIFI, SKIFF_TR(app_weather),   [this] { router().push("weather"); }},
                {ICON_BATT, SKIFF_TR(app_calculator), [this] { router().push("calculator"); }},
                {ICON_APPS, SKIFF_TR(app_files),   [this] { router().push("files"); }},
                {ICON_BT,   SKIFF_TR(app_clock),   [this] { router().push("clock"); }},
            };

            return skiff::VStack({
                skiff::components::TopNav({
                    skiff::components::TopNavView::routerHome(router(), SKIFF_TR(nav_home)).ttf(kFont, 16),
                    skiff::components::TopNavView::routerPrev(router(), SKIFF_TR(nav_back)).ttf(kFont, 16),
                })
                .title(skiff::Text(SKIFF_TR(app_apps)).ttf(kFont, 20))
                .widthPct(100)
                .bg(0x1A222B),
                AppGrid(apps)
                    .cols(2).rows(2)
                    .horizontal()
                    .ttf(kFont, 16)
                    .widthPct(100)
                    .expand()
            }, 0).sizePct(100, 100).bg(kBg);
        };

        router().add("home", {}, homeBody);
        musicPage_ = &router().add("music", {
            skiff::components::state::of<bool>(pnd::music::shuffle, false),
            skiff::components::state::of<int>(pnd::music::repeat, 0),
            skiff::components::state::of<int>(pnd::music::volume, 70),
            skiff::components::state::of<bool>(pnd::music::playing, false),
            skiff::components::state::of<int>(pnd::music::progress, 0),
            skiff::components::state::of<std::string>(
                pnd::music::currentTrack, music_.current().path),
        }, musicBody);
        router().add("games", {}, gamesBody);
        physicsPage_ = &router().add("physics", {
            skiff::components::state::of<int>(pnd::phys::frame, 0),
            skiff::components::state::of<bool>(pnd::phys::paused, false),
            skiff::components::state::of<int>(pnd::phys::shape, 0),
        }, physicsBody);
        router().add("media", {}, mediaBody);
        router().add("apps", {}, appGridBody);
        router().add("settings", {
            skiff::components::state::of<int>(pnd::settings::tab, 0),
        }, settingsBody);
        router().fallback([this]() -> Element { return subPage(router()); });
    }

    void setupOverlay_() {
        router().setOverlayBuilder([this]() -> std::vector<Element> {
            State<bool>& menuExpanded = states().get<bool>(pnd::g::menuExpanded);
            State<int>& brightness = states().get<int>(pnd::g::brightness);
            std::vector<Element> out;
            out.push_back(skiff::Watch(menuExpanded, [this, &brightness](bool expanded) -> Element {
                if (!expanded) {
                    return skiff::VStack(std::vector<Element>(), 0)
                        .size(0, 0)
                        .floating();
                }
                return skiff::VStack({
                    skiff::TapArea([this] {
                        states().get<bool>(pnd::g::menuExpanded).set(false);
                    })
                        .sizePct(100, 100)
                        .floating(),
                    skiff::Watch(brightness, [this](int) -> Element {
                        return topMenuOverlay(states().get<bool>(pnd::g::menuExpanded),
                                              states().get<int>(pnd::g::brightness),
                                              router(),
                                              [this](int v) { display_.setBrightness(v); });
                    }),
                }, 0)
                    .sizePct(100, 100)
                    .floating();
            }));
            return out;
        });
    }

    // Scene → 本页 State 投影。点击只调 physics_ 命令,不要反过来 set 这些键。
    void syncPhysicsUi_() {
        if (!physicsPage_) return;
        components::StateView& st = physicsPage_->stateView();
        st.get<int>(pnd::phys::frame).setIfChanged((int)physics_.frame());
        st.get<bool>(pnd::phys::paused).setIfChanged(physics_.paused());
        st.get<int>(pnd::phys::shape).setIfChanged(physics_.dropCircle() ? 1 : 0);
    }

    void syncMusicUi_() {
        if (!musicPage_) return;
        components::StateView& st = musicPage_->stateView();
        const char* path = music_.current().path;
        st.get<std::string>(pnd::music::currentTrack)
            .setIfChanged(std::string(path ? path : ""));
        st.get<bool>(pnd::music::playing).setIfChanged(music_.playing());
        st.get<int>(pnd::music::progress).setIfChanged(music_.progress());
        st.get<bool>(pnd::music::shuffle).setIfChanged(music_.shuffle());
        st.get<int>(pnd::music::repeat).setIfChanged(music_.repeat() ? 1 : 0);
        st.get<int>(pnd::music::volume).setIfChanged(music_.volume());
    }

    pnd::PlatformMusicSink musicSink_;
    pnd::PlatformDisplay display_;
    app::MusicScene music_;
    app::SceneHost scenes_;
    app::PhysicsScene physics_;
    std::chrono::steady_clock::time_point tickLast_;
    components::PageView* physicsPage_;
    components::PageView* musicPage_;
};

} // namespace demo
} // namespace skiff
