// pnd_sdl:车机 PND 风格主页 —— DSL 编写,平台无关。
//
// 本文件只描述 UI,不感知具体后端(LVGL/SDL3)或运行平台。
// 平台相关入口(如 macOS)在 platforms/mac/mac_platform.cpp 中实现。
// 文案经 pnd_i18n(业务) + skiff::i18n(框架);路由使用稳定英文 ID。
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <string>

#include "skiff/skiff.hpp"
#include "pnd_i18n.hpp"
#include "physics_sim.hpp"

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
                       skiff::Platform& platform) {
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
                 [&platform, &brightness](int v) {
                     brightness.set(v);
                     platform.invokeExternal("setBrightness",
                                            {std::to_string(v)});
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

Element brightnessRow(int brightness, State<int>& brightnessState) {
    return skiff::HStack({
        skiff::Text(SKIFF_TR(app_brightness)).ttf(kFont, 18).fg(kHi),
        skiff::Spacer(),
        skiff::Slider(brightness, 0, 100,
                      [&brightnessState](int v) { brightnessState.set(v); })
            .size(180, 24),
        skiff::Text(std::to_string(brightness) + "%")
            .ttf(kFont, 16).fg(kLo).size(48, 24),
    }, 12).size(0, 48).widthPct(100).centered();
}

// ---- 音乐播放:横屏 Now Playing(参考 CarPlay / Apple Music) ----

struct TrackMeta {
    const char* path;
    const char* title;
    const char* artist;
    int durationSec;
    uint32_t artColor;
};

const TrackMeta kTracks[] = {
    {"assets/music/sample.wav", "Sample", "Demo", 32, 0xD84315},
    {"assets/music/mickeyscat-moment-of-peace-mickeyscat-554494.mp3",
     "Moment of Peace", "Mickeyscat", 214, 0x1B4F72},
};
const int kTrackCount = static_cast<int>(sizeof(kTracks) / sizeof(kTracks[0]));

std::string fileStem(const std::string& path) {
    std::string name = path;
    const std::size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    const std::size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) name = name.substr(0, dot);
    if (name.empty()) name = "Unknown";
    return name;
}

int trackIndexOf(const std::string& path) {
    for (int i = 0; i < kTrackCount; ++i) {
        if (path == kTracks[i].path) return i;
    }
    return 0;
}

const TrackMeta* findTrack(const std::string& path) {
    for (int i = 0; i < kTrackCount; ++i) {
        if (path == kTracks[i].path) return &kTracks[i];
    }
    return 0;
}

std::string trackTitleOf(const std::string& path) {
    const TrackMeta* m = findTrack(path);
    if (m) return m->title;
    return fileStem(path);
}

std::string trackArtistOf(const std::string& path) {
    const TrackMeta* m = findTrack(path);
    if (m) return m->artist;
    return SKIFF_TR(music_unknown_artist);
}

std::string formatClock(int sec) {
    if (sec < 0) sec = 0;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d:%02d", sec / 60, sec % 60);
    return buf;
}

void startMusicTrack(skiff::Platform& platform,
                     State<std::string>& currentTrack,
                     State<bool>& playing,
                     State<int>& progress,
                     const std::string& path) {
    currentTrack.set(path);
    progress.set(0);
    playing.set(true);
    platform.invokeLater("stopMusic", {});
    platform.invokeLater("playMusic", {path});
}

int nextTrackIndex(int current, bool shuffle) {
    if (kTrackCount <= 1) return 0;
    if (!shuffle) return (current + 1) % kTrackCount;
    int n = std::rand() % (kTrackCount - 1);
    if (n >= current) ++n;
    return n;
}

Element musicAlbumArt(const std::string& path) {
    const TrackMeta* meta = findTrack(path);
    const uint32_t color = meta ? meta->artColor : kMusic;
    return skiff::VStack({
        skiff::VStack({
            skiff::Text(ICON_AUDIO).font(72).fg(0xFFFFFF),
        }).size(220, 220).bg(color).radius(16).centered(),
    }).size(236, 236).bg(0x0A0E12).radius(20).centered();
}

Element musicTitleBlock(const std::string& path) {
    return skiff::VStack({
        skiff::Text(SKIFF_TR(music_now_playing)).ttf(kFont, 13).fg(kMusic),
        skiff::Text(trackTitleOf(path)).ttf(kFont, 26).fg(kHi),
        skiff::Text(trackArtistOf(path)).ttf(kFont, 16).fg(kLo),
    }, 4);
}

Element musicProgressRow(int progress, int durationSec, State<int>& progressState) {
    int elapsed = 0;
    if (durationSec > 0) elapsed = progress * durationSec / 100;
    if (elapsed > durationSec) elapsed = durationSec;
    return skiff::HStack({
        skiff::Text(formatClock(elapsed)).ttf(kFont, 12).fg(kLo).size(44, 16),
        skiff::Slider(progress, 0, 100,
                      [&progressState](int v) { progressState.set(v); })
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

Element musicPlayButton(bool playing, skiff::Platform& platform,
                        State<bool>& playingState,
                        State<std::string>& trackState) {
    return skiff::Button({skiff::Text(playing ? ICON_PAUSE : ICON_PLAY)
                              .font(28)
                              .fg(0x1A1A1A)},
                         [&platform, &playingState, &trackState] {
                             if (playingState.get()) {
                                 platform.invokeLater("stopMusic", {});
                                 playingState.set(false);
                             } else {
                                 platform.invokeLater(
                                     "playMusic", {trackState.get()});
                                 playingState.set(true);
                             }
                         })
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

Element displaySubmenu(int brightness, State<int>& brightnessState) {
    return skiff::VStack({
        brightnessRow(brightness, brightnessState),
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
// 这里只写 PND 特有的部分(能力声明、事件订阅、页面与下拉菜单)。
class PndUi : public components::AppUi {
public:
    explicit PndUi(Platform& platform)
        : components::AppUi(platform, "home"),
          sim_(kPhysCanvasW, kPhysCanvasH),
          physicsAcc_(0.0f),
          physicsPage_(0) {
        physicsLast_ = std::chrono::steady_clock::now();
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
        setupOverlay_();
    }

    void toggleMenu() {
        State<bool>& e = states().get<bool>("menuExpanded");
        e.set(!e.get());
    }

    void tick() {
        const std::chrono::steady_clock::time_point now =
            std::chrono::steady_clock::now();
        if (router().current() != "physics" || physicsPage_ == 0) {
            physicsLast_ = now;
            physicsAcc_ = 0.0f;
            return;
        }
        float dt = std::chrono::duration<float>(now - physicsLast_).count();
        physicsLast_ = now;
        if (dt > 0.25f) dt = 0.25f;
        physicsAcc_ += dt;

        State<bool>& paused = physicsPage_->stateView().get<bool>("paused");
        State<int>& frame = physicsPage_->stateView().get<int>("frame");
        const float step = 1.0f / 60.0f;
        bool dirty = false;
        while (physicsAcc_ >= step) {
            physicsAcc_ -= step;
            if (!paused.get()) {
                sim_.step(step);
                dirty = true;
            }
        }
        if (dirty) frame.set(frame.get() + 1);
    }

private:
    void applyLocale_(const std::string& next) {
        skiff::i18n::setLocale(next);
        states().get<std::string>("locale").set(next);
    }

    void setupPages_() {
        auto musicBody = [this](components::StateView& st) -> Element {
            State<int>& musicProgress_ = states().get<int>("musicProgress");
            State<bool>& musicPlaying_ = states().get<bool>("musicPlaying");
            State<std::string>& currentTrack_ =
                states().get<std::string>("currentTrack");
            State<bool>& shuffle_ = st.get<bool>("shuffle");
            State<int>& repeat_ = st.get<int>("repeat");
            State<int>& volume_ = st.get<int>("volume");

            Element album = skiff::Watch(currentTrack_, [](const std::string& track) {
                return musicAlbumArt(track);
            });

            Element titles = skiff::Watch(currentTrack_, [](const std::string& track) {
                return musicTitleBlock(track);
            });

            Element progressRow = skiff::Watch(
                currentTrack_, [&musicProgress_](const std::string& track) {
                    const TrackMeta* meta = findTrack(track);
                    const int duration = meta ? meta->durationSec : 180;
                    return skiff::Watch(
                        musicProgress_, [duration, &musicProgress_](int p) {
                            return musicProgressRow(p, duration, musicProgress_);
                        });
                });

            Element shuffleBtn = skiff::Watch(shuffle_, [&shuffle_](bool on) {
                return musicIconBtn(ICON_SHUFFLE, on ? kMusic : kLo, 48,
                                    [&shuffle_] { shuffle_.set(!shuffle_.get()); });
            });

            Element prevBtn = musicIconBtn(
                ICON_PREV, kHi, 52, [this, &currentTrack_, &musicPlaying_,
                                     &musicProgress_] {
                    const int p = musicProgress_.get();
                    if (p > 5) {
                        startMusicTrack(platform(), currentTrack_, musicPlaying_,
                                        musicProgress_, currentTrack_.get());
                        return;
                    }
                    const int i = trackIndexOf(currentTrack_.get());
                    const int prev = (i - 1 + kTrackCount) % kTrackCount;
                    startMusicTrack(platform(), currentTrack_, musicPlaying_,
                                    musicProgress_, kTracks[prev].path);
                });

            Element playBtn = skiff::Watch(
                musicPlaying_, [this, &musicPlaying_, &currentTrack_](bool playing) {
                    return musicPlayButton(playing, platform(), musicPlaying_,
                                           currentTrack_);
                });

            Element nextBtn = musicIconBtn(
                ICON_NEXT, kHi, 52, [this, &currentTrack_, &musicPlaying_,
                                     &musicProgress_, &shuffle_] {
                    const int i = trackIndexOf(currentTrack_.get());
                    const int n = nextTrackIndex(i, shuffle_.get());
                    startMusicTrack(platform(), currentTrack_, musicPlaying_,
                                    musicProgress_, kTracks[n].path);
                });

            Element repeatBtn = skiff::Watch(repeat_, [&repeat_](int mode) {
                return musicIconBtn(ICON_LOOP, mode != 0 ? kMusic : kLo, 48,
                                    [&repeat_] { repeat_.set(repeat_.get() == 0 ? 1 : 0); });
            });

            Element controls = skiff::HStack({
                shuffleBtn,
                prevBtn,
                playBtn,
                nextBtn,
                repeatBtn,
            }, 16).centered();

            Element volumeRow = skiff::Watch(volume_, [&volume_](int v) {
                return skiff::HStack({
                    skiff::Text(ICON_VOLUME).font(16).fg(kLo),
                    skiff::Slider(v, 0, 100, [&volume_](int n) { volume_.set(n); })
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
                    .sizePct(100, 100);
            };

            std::vector<skiff::components::ListItem> videoItems = {
                {"sample video",
                 [this] { platform().invokeExternal("openFile", {"assets/media/sample.mp4"}); }},
            };
            std::vector<skiff::components::ListItem> musicItems = {
                {"Moment of Peace", "Mickeyscat",
                 [this] {
                     const std::string track = kTracks[1].path;
                     startMusicTrack(platform(),
                                     states().get<std::string>("currentTrack"),
                                     states().get<bool>("musicPlaying"),
                                     states().get<int>("musicProgress"),
                                     track);
                     router().push("music");
                 }},
                {"Sample", "Demo",
                 [this] {
                     const std::string track = kTracks[0].path;
                     startMusicTrack(platform(),
                                     states().get<std::string>("currentTrack"),
                                     states().get<bool>("musicPlaying"),
                                     states().get<int>("musicProgress"),
                                     track);
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
            State<int>& frame = st.get<int>("frame");
            State<bool>& paused = st.get<bool>("paused");
            State<int>& shape = st.get<int>("shape");

            Element canvas = skiff::Watch(frame, [this, &frame, &shape](int) -> Element {
                return skiff::Canvas(kPhysCanvasW, kPhysCanvasH,
                                     [this](skiff::CanvasContext& c) {
                                         sim_.paint(c);
                                     })
                    .onTapAt([this, &frame, &shape](int x, int y) {
                        sim_.spawnAtCanvas(x, y, shape.get() != 0);
                        frame.set(frame.get() + 1);
                    });
            });

            Element side = skiff::VStack({
                skiff::Text(SKIFF_TR(physics_hint))
                    .ttf(kFont, 14)
                    .fg(kLo)
                    .widthPct(100),
                skiff::Watch(paused, [&paused](bool p) -> Element {
                    return physicsToolBtn(p ? SKIFF_TR(physics_resume)
                                            : SKIFF_TR(physics_pause),
                                          [&paused] { paused.set(!paused.get()); });
                }),
                skiff::Watch(shape, [&shape](int s) -> Element {
                    return physicsToolBtn(s == 0 ? SKIFF_TR(physics_drop_box)
                                                 : SKIFF_TR(physics_drop_ball),
                                          [&shape, s] { shape.set(s == 0 ? 1 : 0); });
                }),
                physicsToolBtn(SKIFF_TR(physics_reset), [this, &frame] {
                    sim_.reset();
                    frame.set(frame.get() + 1);
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
            State<int>& tab = st.get<int>("tab");
            State<int>& brightness = states().get<int>("brightness");
            State<std::string>& locale = states().get<std::string>("locale");

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
                    {SKIFF_TR(settings_display), skiff::Watch(brightness, [&brightness](int v) {
                        return displaySubmenu(v, brightness);
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
        router().add("music", {
            skiff::components::state::of<bool>("shuffle", false),
            skiff::components::state::of<int>("repeat", 0),
            skiff::components::state::of<int>("volume", 70),
        }, musicBody);
        router().add("games", {}, gamesBody);
        physicsPage_ = &router().add("physics", {
            skiff::components::state::of<int>("frame", 0),
            skiff::components::state::of<bool>("paused", false),
            skiff::components::state::of<int>("shape", 0),
        }, physicsBody);
        router().add("media", {}, mediaBody);
        router().add("apps", {}, appGridBody);
        router().add("settings", {
            skiff::components::state::of<int>("tab", 0),
        }, settingsBody);
        router().fallback([this]() -> Element { return subPage(router()); });
    }

    void setupOverlay_() {
        router().setOverlayBuilder([this]() -> std::vector<Element> {
            State<bool>& menuExpanded = states().get<bool>("menuExpanded");
            State<int>& brightness = states().get<int>("brightness");
            std::vector<Element> out;
            out.push_back(skiff::Watch(menuExpanded, [this, &brightness](bool expanded) -> Element {
                if (!expanded) {
                    return skiff::VStack(std::vector<Element>(), 0)
                        .size(0, 0)
                        .floating();
                }
                return skiff::VStack({
                    skiff::TapArea([this] {
                        states().get<bool>("menuExpanded").set(false);
                    })
                        .sizePct(100, 100)
                        .floating(),
                    skiff::Watch(brightness, [this](int) -> Element {
                        return topMenuOverlay(states().get<bool>("menuExpanded"),
                                              states().get<int>("brightness"),
                                              router(), platform());
                    }),
                }, 0)
                    .sizePct(100, 100)
                    .floating();
            }));
            return out;
        });
    }

    pnd::physics::Sim sim_;
    float physicsAcc_;
    std::chrono::steady_clock::time_point physicsLast_;
    components::PageView* physicsPage_;
};

} // namespace demo
} // namespace skiff
