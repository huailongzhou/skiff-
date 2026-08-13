// Windows 平台入口:把平台无关的 PND UI 挂载到 LVGL+SDL3,并注册 Windows 平台能力。
//
// 用法(Windows, MSVC 或 MinGW):
//   mkdir build && cd build
//   cmake .. && cmake --build . --target pnd_win
//   ./pnd_win
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include <windows.h>
#include <mmsystem.h>  // mciSendStringA(winmm.lib)

#include "skiff_lvgl.hpp"
#include "skiff_lvgl_sdl3.hpp"
#include "skiff/skiff.hpp"
#include "examples/pnd_sdl.cpp"

// 简单的 Windows 音乐播放器:用 MCI(winmm)播放本地音频文件
namespace {

class MciMusicPlayer {
public:
    bool play(const std::string& path) {
        stop();
        const std::string open = "open \"" + path + "\" alias skiffmusic";
        if (mciSendStringA(open.c_str(), nullptr, 0, nullptr) != 0) {
            playing_ = false;
            return false;
        }
        playing_ = (mciSendStringA("play skiffmusic", nullptr, 0, nullptr) == 0);
        return playing_;
    }

    void stop() {
        if (playing_) {
            mciSendStringA("close skiffmusic", nullptr, 0, nullptr);
            playing_ = false;
        }
    }

    bool isPlaying() const { return playing_; }

private:
    bool playing_ = false;
};

MciMusicPlayer gMusicPlayer;

// 设置内置屏幕亮度(0~100),走 WMI(仅笔记本/内置屏有效)。返回是否成功。
bool setWinBrightness(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    std::stringstream ss;
    ss << "powershell -NoProfile -Command \""
       << "(Get-WmiObject -Namespace root/wmi -Class WmiMonitorBrightnessMethods)"
       << ".WmiSetBrightness(1," << pct << ")\" >NUL 2>&1";
    return std::system(ss.str().c_str()) == 0;
}

// 注册所有声明的 Windows 平台能力实现
void registerWinPlatform(skiff::Platform& platform) {
    if (platform.hasDeclared("setBrightness")) {
        platform.registerExternal("setBrightness",
            [](const std::vector<std::string>& args) {
                if (args.empty()) return;
                const int pct = std::atoi(args[0].c_str());
                if (setWinBrightness(pct)) {
                    std::printf("[WinPlatform] setBrightness %d%%\n", pct);
                } else {
                    std::printf("[WinPlatform] setBrightness %d%% failed\n", pct);
                }
            });
    }

    if (platform.hasDeclared("playMusic")) {
        platform.registerExternal("playMusic",
            [](const std::vector<std::string>& args) {
                const std::string path = args.empty() ? "" : args[0];
                if (!path.empty()) {
                    if (gMusicPlayer.play(path)) {
                        std::printf("[WinPlatform] playMusic %s\n", path.c_str());
                    } else {
                        std::printf("[WinPlatform] playMusic %s failed\n", path.c_str());
                    }
                }
            });
    }

    if (platform.hasDeclared("stopMusic")) {
        platform.registerExternal("stopMusic",
            [](const std::vector<std::string>&) {
                gMusicPlayer.stop();
                std::printf("[WinPlatform] stopMusic\n");
            });
    }

    if (platform.hasDeclared("setVolume")) {
        platform.registerExternal("setVolume",
            [](const std::vector<std::string>& args) {
                if (args.empty()) return;
                int pct = std::atoi(args[0].c_str());
                if (pct < 0) pct = 0;
                if (pct > 100) pct = 100;
                const DWORD level = (DWORD)(pct * 0xFFFF / 100);
                waveOutSetVolume(0, (level << 16) | level);
                std::printf("[WinPlatform] setVolume %d%%\n", pct);
            });
    }
}

} // namespace

int main() {
    lv_init();
    skiff::lvgl::createSdl3Display(800, 480, "skiff - PND home (Windows)");

    skiff::Platform platform;
    skiff::demo::PndUi ui(platform);
    registerWinPlatform(platform);

    skiff::lvgl::LvglBackend backend(lv_scr_act());
    skiff::App app(backend, [&ui]() -> skiff::Element { return ui.render(); });
    ui.bind(app);

    // 从顶部状态栏向下滑动手势:唤出/收起下拉菜单
    const int kTopZone = 24;
    const int kSwipeThreshold = 80;
    skiff::input::onSwipeDown(kTopZone, kSwipeThreshold,
                              [&ui]() { ui.toggleMenu(); });

    skiff::lvgl::run(app, &platform);
    skiff::lvgl::destroySdl3Display();
    return 0;
}
