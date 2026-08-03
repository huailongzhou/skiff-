// macOS 平台入口:把平台无关的 PND UI 挂载到 LVGL+SDL3,并注册 macOS 平台能力。
//
// 用法:
//   mkdir -p build && cd build
//   cmake .. && make pnd_mac
//   ./pnd_mac
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <CoreGraphics/CoreGraphics.h>

#include "skiff_lvgl.hpp"
#include "skiff_lvgl_sdl3.hpp"
#include "skiff/skiff.hpp"
#include "examples/pnd_sdl.cpp"

// 简单的 macOS 音乐播放器:用 afplay 播放本地音频文件
namespace {

class AfplayMusicPlayer {
public:
    void play(const std::string& path) {
        stop();
        std::string cmd = "afplay \"" + path + "\" &";
        std::system(cmd.c_str());
        playing_ = true;
    }

    void stop() {
        std::system("killall afplay 2>/dev/null");
        playing_ = false;
    }

    bool isPlaying() const { return playing_; }

private:
    bool playing_ = false;
};

AfplayMusicPlayer gMusicPlayer;

} // namespace

namespace {

// DisplayServices 私有框架函数(控制内置屏幕亮度)
extern "C" void DisplayServicesSetBrightness(CGDirectDisplayID display, double brightness);

// 设置主屏幕亮度(0.0 ~ 1.0)。返回是否成功。
bool setMacBrightness(float level) {
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;

    CGDirectDisplayID display = CGMainDisplayID();
    if (display == kCGNullDirectDisplay) return false;

    DisplayServicesSetBrightness(display, static_cast<double>(level));
    return true;
}

// 注册所有声明的 macOS 平台能力实现
void registerMacPlatform(skiff::Platform& platform) {
    if (platform.hasDeclared("setBrightness")) {
        platform.registerExternal("setBrightness",
            [](const std::vector<std::string>& args) {
                if (args.empty()) return;
                const int pct = std::atoi(args[0].c_str());
                const float level = pct / 100.0f;
                if (setMacBrightness(level)) {
                    std::printf("[MacPlatform] setBrightness %d%%\n", pct);
                } else {
                    std::printf("[MacPlatform] setBrightness %d%% failed\n", pct);
                }
            });
    }

    if (platform.hasDeclared("playMusic")) {
        platform.registerExternal("playMusic",
            [](const std::vector<std::string>& args) {
                const std::string path = args.empty() ? "" : args[0];
                if (!path.empty()) {
                    gMusicPlayer.play(path);
                    std::printf("[MacPlatform] playMusic %s\n", path.c_str());
                }
            });
    }

    if (platform.hasDeclared("stopMusic")) {
        platform.registerExternal("stopMusic",
            [](const std::vector<std::string>&) {
                gMusicPlayer.stop();
                std::printf("[MacPlatform] stopMusic\n");
            });
    }
}

} // namespace

int main() {
    lv_init();
    skiff::lvgl::createSdl3Display(800, 480, "skiff - PND home (macOS)");

    skiff::Platform platform;
    skiff::demo::PndUi ui(platform);
    registerMacPlatform(platform);

    skiff::lvgl::LvglBackend backend(lv_scr_act());
    skiff::App app(backend, [&ui]() -> skiff::Element { return ui.render(); });
    ui.bind(app);

    // 从顶部状态栏向下滑动手势:唤出/收起下拉菜单
    const int kTopZone = 24;
    const int kSwipeThreshold = 80;
    skiff::input::onSwipeDown(kTopZone, kSwipeThreshold,
                              [&ui]() { ui.toggleMenu(); });

    skiff::lvgl::run(app);
    skiff::lvgl::destroySdl3Display();
    return 0;
}
