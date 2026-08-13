// macOS 平台入口:把平台无关的 PND UI 挂载到 LVGL+SDL3,并注册 macOS 平台能力。
//
// 用法:
//   mkdir -p build && cd build
//   cmake .. && make pnd_mac
//   ./pnd_mac
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>
#include <CoreGraphics/CoreGraphics.h>

#include "skiff_lvgl.hpp"
#include "skiff_lvgl_sdl3.hpp"
#include "skiff/skiff.hpp"
#include "examples/pnd_sdl.cpp"

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

// macOS 音乐播放器:直接用 SDL3 音频播放 wav(与 Linux 入口保持一致),
// 支持进度上报;SDL 音频不可用时回退到 afplay。
class SdlMusicPlayer {
public:
    ~SdlMusicPlayer() { stop(); }

    bool play(const std::string& path) {
        stop();
        if (!SDL_WasInit(SDL_INIT_AUDIO)) SDL_InitSubSystem(SDL_INIT_AUDIO);

        SDL_AudioSpec spec;
        if (!SDL_LoadWAV(path.c_str(), &spec, &buf_, &len_)) {
            std::printf("[MacPlatform] SDL_LoadWAV failed: %s\n", SDL_GetError());
            buf_ = nullptr;
            return false;
        }
        stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                            &spec, nullptr, nullptr);
        if (!stream_) {
            std::printf("[MacPlatform] open audio device failed: %s\n",
                        SDL_GetError());
            SDL_free(buf_);
            buf_ = nullptr;
            return false;
        }
        SDL_PutAudioStreamData(stream_, buf_, (int)len_);
        SDL_ResumeAudioStreamDevice(stream_);
        playing_ = true;
        return true;
    }

    void stop() {
        if (stream_) { SDL_DestroyAudioStream(stream_); stream_ = nullptr; }
        if (buf_) { SDL_free(buf_); buf_ = nullptr; }
        playing_ = false;
        // 回退路径可能起过 afplay,一并清掉(无害)
        std::system("killall afplay 2>/dev/null");
    }

    bool isPlaying() {
        if (playing_ && stream_ && SDL_GetAudioStreamQueued(stream_) == 0) {
            stop();
        }
        return playing_;
    }

    int progressPct() const {
        if (!playing_ || !stream_ || len_ == 0) return -1;
        const int queued = SDL_GetAudioStreamQueued(stream_);
        int pct = (int)((len_ - (Uint32)queued) * 100 / len_);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        return pct;
    }

private:
    SDL_AudioStream* stream_ = nullptr;
    Uint8* buf_ = nullptr;
    Uint32 len_ = 0;
    bool playing_ = false;
};

SdlMusicPlayer gMusicPlayer;

// 播放监控:每 500ms 检查进度,经平台事件上报给 UI(在独立线程运行)。
void musicMonitorLoop(skiff::Platform* platform, std::atomic<bool>* running) {
    int lastPct = -1;
    bool wasPlaying = false;
    while (running->load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        const int pct = gMusicPlayer.progressPct();
        if (pct >= 0 && pct != lastPct) {
            lastPct = pct;
            platform->emit("musicProgress", {std::to_string(pct)});
        }
        const bool playing = gMusicPlayer.isPlaying();
        if (wasPlaying && !playing) platform->emit("musicEnded", {});
        wasPlaying = playing;
        if (!playing) lastPct = -1;
    }
}

// afplay 回退:SDL 音频打不开时用系统命令行播放
void fallbackAfplay(const std::string& path) {
    std::string cmd = "afplay \"" + path + "\" >/dev/null 2>&1 &";
    std::system(cmd.c_str());
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
                if (path.empty()) return;
                if (gMusicPlayer.play(path)) {
                    std::printf("[MacPlatform] playMusic %s (SDL audio)\n",
                                path.c_str());
                } else {
                    fallbackAfplay(path);
                    std::printf("[MacPlatform] playMusic %s (afplay fallback)\n",
                                path.c_str());
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

    if (platform.hasDeclared("openFile")) {
        platform.registerExternal("openFile",
            [](const std::vector<std::string>& args) {
                if (args.empty()) return;
                const std::string& path = args[0];
                std::string cmd = "open \"" + path + "\"";
                std::system(cmd.c_str());
                std::printf("[MacPlatform] openFile %s\n", path.c_str());
            });
    }

    if (platform.hasDeclared("setVolume")) {
        platform.registerExternal("setVolume",
            [](const std::vector<std::string>& args) {
                if (args.empty()) return;
                int pct = std::atoi(args[0].c_str());
                if (pct < 0) pct = 0;
                if (pct > 100) pct = 100;
                std::string cmd =
                    "osascript -e 'set volume output volume " +
                    std::to_string(pct) + "' >/dev/null 2>&1";
                std::system(cmd.c_str());
                std::printf("[MacPlatform] setVolume %d%%\n", pct);
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

    // 播放进度上报线程:平台事件 → UI
    std::atomic<bool> monitorRunning(true);
    std::thread monitor(musicMonitorLoop, &platform, &monitorRunning);

    skiff::lvgl::run(app, &platform);  // 主循环每帧 pumpEvents 派发平台事件

    monitorRunning = false;
    monitor.join();
    gMusicPlayer.stop();  // 先停音频,再销毁 SDL
    skiff::lvgl::destroySdl3Display();
    return 0;
}
