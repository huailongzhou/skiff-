// Linux 平台入口:把平台无关的 PND UI 挂载到 LVGL+SDL3,并注册 Linux 平台能力。
//
// 用法:
//   cmake -S . -B build && cmake --build build --target pnd_linux -j
//   ./build/pnd_linux
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <dirent.h>
#include <mpg123.h>

#include <SDL3/SDL.h>

#include "skiff_lvgl.hpp"
#include "skiff_lvgl_sdl3.hpp"
#include "skiff/skiff.hpp"
#include "examples/pnd_sdl.cpp"

namespace {

// 用 libmpg123 把 mp3 整首解码为 PCM(S16),供 SDL 音频流播放。
// 返回 false 表示不是 mp3 或解码失败。
bool loadMp3(const std::string& path, SDL_AudioSpec* spec,
             Uint8** buf, Uint32* len) {
    static bool mpgReady = false;
    if (!mpgReady) mpgReady = (mpg123_init() == MPG123_OK);
    if (!mpgReady) return false;

    int err = 0;
    mpg123_handle* mh = mpg123_new(nullptr, &err);
    if (mh == nullptr) return false;
    if (mpg123_open(mh, path.c_str()) != MPG123_OK) {
        mpg123_delete(mh);
        return false;
    }

    long rate = 0;
    int channels = 0, encoding = 0;
    if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK ||
        encoding != MPG123_ENC_SIGNED_16) {
        mpg123_close(mh);
        mpg123_delete(mh);
        return false;
    }
    spec->freq = (int)rate;
    spec->channels = (Uint8)channels;
    spec->format = SDL_AUDIO_S16LE;

    std::vector<Uint8> pcm;
    unsigned char chunk[16384];
    size_t n = 0;
    int r;
    int idle = 0;
    const size_t kMaxPcm = 80u * 1024u * 1024u;
    while ((r = mpg123_read(mh, chunk, sizeof(chunk), &n)) == MPG123_OK ||
           r == MPG123_NEW_FORMAT) {
        if (n == 0) {
            if (++idle > 8) break;
            continue;
        }
        idle = 0;
        if (pcm.size() + n > kMaxPcm) break;
        pcm.insert(pcm.end(), chunk, chunk + n);
    }
    mpg123_close(mh);
    mpg123_delete(mh);
    if (pcm.empty()) return false;

    *len = (Uint32)pcm.size();
    *buf = (Uint8*)SDL_malloc(*len);
    if (*buf == nullptr) return false;
    memcpy(*buf, pcm.data(), *len);
    return true;
}

bool endsWithWav(const std::string& path) {
    if (path.size() < 4) return false;
    std::string ext = path.substr(path.size() - 4);
    for (size_t i = 0; i < ext.size(); ++i) {
        if (ext[i] >= 'A' && ext[i] <= 'Z') ext[i] += 'a' - 'A';
    }
    return ext == ".wav";
}

// Linux 音乐播放器:wav 用 SDL_LoadWAV,mp3 用 libmpg123 解码,
// 统一走 SDL3 音频流播放,不依赖外部命令;
// SDL 音频不可用时(无声卡/无音频服务)回退到 aplay。
class SdlMusicPlayer {
public:
    SdlMusicPlayer() : stream_(nullptr), buf_(nullptr), len_(0), playing_(false) {}
    ~SdlMusicPlayer() { stop(); }

    bool play(const std::string& path) {
        stop();
        if (!SDL_WasInit(SDL_INIT_AUDIO)) SDL_InitSubSystem(SDL_INIT_AUDIO);

        SDL_AudioSpec spec;
        Uint8* buf = nullptr;
        Uint32 len = 0;
        if (endsWithWav(path)) {
            if (!SDL_LoadWAV(path.c_str(), &spec, &buf, &len)) {
                std::printf("[LinuxPlatform] SDL_LoadWAV failed: %s\n",
                            SDL_GetError());
                return false;
            }
        } else if (!loadMp3(path, &spec, &buf, &len)) {
            std::printf("[LinuxPlatform] load mp3 failed: %s\n", path.c_str());
            return false;
        }
        SDL_AudioStream* stream =
            SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                      &spec, nullptr, nullptr);
        if (!stream) {
            std::printf("[LinuxPlatform] open audio device failed: %s\n",
                        SDL_GetError());
            SDL_free(buf);
            return false;
        }
        SDL_PutAudioStreamData(stream, buf, (int)len);
        SDL_ResumeAudioStreamDevice(stream);
        {
            std::lock_guard<std::mutex> lk(mu_);
            stopLocked();
            stream_ = stream;
            buf_ = buf;
            len_ = len;
            playing_ = true;
        }
        return true;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lk(mu_);
            stopLocked();
        }
        std::system("killall aplay 2>/dev/null");
    }

    bool isPlaying() {
        std::lock_guard<std::mutex> lk(mu_);
        if (playing_ && stream_ && SDL_GetAudioStreamQueued(stream_) == 0) {
            stopLocked();
        }
        return playing_;
    }

    int progressPct() const {
        std::lock_guard<std::mutex> lk(mu_);
        if (!playing_ || !stream_ || len_ == 0) return -1;
        const int queued = SDL_GetAudioStreamQueued(stream_);
        int pct = (int)((len_ - (Uint32)queued) * 100 / len_);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        return pct;
    }

private:
    void stopLocked() {
        if (stream_) { SDL_DestroyAudioStream(stream_); stream_ = nullptr; }
        if (buf_) { SDL_free(buf_); buf_ = nullptr; }
        len_ = 0;
        playing_ = false;
    }

    mutable std::mutex mu_;
    SDL_AudioStream* stream_;
    Uint8* buf_;
    Uint32 len_;
    bool playing_;
};

SdlMusicPlayer gMusicPlayer;

void fallbackAplay(const std::string& path);

// 解码 MP3 / 打开声卡放到工作线程,避免卡住 LVGL 主循环。
class MusicService {
public:
    struct Cmd {
        bool stop;
        std::string path;
        Cmd() : stop(true) {}
        Cmd(bool s, const std::string& p) : stop(s), path(p) {}
    };

    MusicService() : running_(true) {}
    ~MusicService() { shutdown(); }

    void requestPlay(const std::string& path) {
        std::lock_guard<std::mutex> lk(mu_);
        ensureThreadLocked();
        q_.clear();
        q_.push_back(Cmd(false, path));
        cv_.notify_one();
    }

    void requestStop() {
        std::lock_guard<std::mutex> lk(mu_);
        ensureThreadLocked();
        q_.clear();
        q_.push_back(Cmd(true, std::string()));
        cv_.notify_one();
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (!running_) return;
            running_ = false;
            cv_.notify_one();
        }
        if (th_.joinable()) th_.join();
    }

private:
    void ensureThreadLocked() {
        if (!th_.joinable() && running_) {
            th_ = std::thread(&MusicService::loop, this);
        }
    }

    void loop() {
        for (;;) {
            Cmd cmd;
            {
                std::unique_lock<std::mutex> lk(mu_);
                while (q_.empty() && running_) cv_.wait(lk);
                if (!running_ && q_.empty()) return;
                if (q_.empty()) return;
                cmd = q_.front();
                q_.erase(q_.begin());
            }
            if (cmd.stop) {
                gMusicPlayer.stop();
                std::printf("[LinuxPlatform] stopMusic\n");
            } else if (gMusicPlayer.play(cmd.path)) {
                std::printf("[LinuxPlatform] playMusic %s (SDL audio)\n",
                            cmd.path.c_str());
            } else {
                fallbackAplay(cmd.path);
                std::printf("[LinuxPlatform] playMusic %s (aplay fallback)\n",
                            cmd.path.c_str());
            }
        }
    }

    std::mutex mu_;
    std::condition_variable cv_;
    std::vector<Cmd> q_;
    bool running_;
    std::thread th_;
};

MusicService gMusicService;

// 播放监控:每 500ms 检查进度,经平台事件上报给 UI(在独立线程运行)。
// 进度变化时 emit "musicProgress" [pct];播放自然结束时 emit "musicEnded"。
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

// aplay 回退:SDL 音频打不开时用 ALSA 命令行播放
void fallbackAplay(const std::string& path) {
    std::string cmd = "aplay -q \"" + path + "\" &";
    std::system(cmd.c_str());
}

// 设置屏幕亮度(0~100):写 /sys/class/backlight/<设备>/brightness。
// 需要当前用户对该节点有写权限;无背光设备(如台式机/VM)时返回 false。
bool setLinuxBrightness(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    DIR* dir = opendir("/sys/class/backlight");
    if (!dir) return false;
    std::string dev;
    while (dirent* ent = readdir(dir)) {
        if (ent->d_name[0] != '.') { dev = ent->d_name; break; }
    }
    closedir(dir);
    if (dev.empty()) return false;

    const std::string base = "/sys/class/backlight/" + dev + "/";
    int maxVal = 0;
    if (FILE* f = std::fopen((base + "max_brightness").c_str(), "r")) {
        if (std::fscanf(f, "%d", &maxVal) != 1) maxVal = 0;
        std::fclose(f);
    }
    if (maxVal <= 0) return false;

    FILE* f = std::fopen((base + "brightness").c_str(), "w");
    if (!f) return false;
    std::fprintf(f, "%d", maxVal * pct / 100);
    std::fclose(f);
    return true;
}

// 是否 WSL 环境(osrelease 含 microsoft/WSL)
bool isWsl() {
    FILE* f = std::fopen("/proc/sys/kernel/osrelease", "r");
    if (!f) return false;
    char buf[256] = {0};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    if (n == 0) return false;
    const std::string s(buf);
    return s.find("microsoft") != std::string::npos ||
           s.find("WSL") != std::string::npos;
}

// WSL 下无 /sys/class/backlight:经 WSL 互操作调 Windows WMI 亮度接口。
// 仅笔记本内置屏有效;台式机/外接显示器(WMI 不支持)返回 false。
bool setWslBrightness(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    std::stringstream ss;
    ss << "/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"
       << " -NoProfile -Command \""
       << "(Get-WmiObject -Namespace root/wmi -Class WmiMonitorBrightnessMethods)"
       << ".WmiSetBrightness(1," << pct << ")\" >/dev/null 2>&1";
    return std::system(ss.str().c_str()) == 0;
}

// 亮度设置入口:真 Linux 走 sysfs;WSL 走 Windows WMI
bool setBrightness(int pct) {
    if (isWsl()) return setWslBrightness(pct);
    return setLinuxBrightness(pct);
}

// 注册所有声明的 Linux 平台能力实现
void registerLinuxPlatform(skiff::Platform& platform) {
    if (platform.hasDeclared("setBrightness")) {
        platform.registerExternal("setBrightness",
            [](const std::vector<std::string>& args) {
                if (args.empty()) return;
                const int pct = std::atoi(args[0].c_str());
                if (setBrightness(pct)) {
                    std::printf("[LinuxPlatform] setBrightness %d%%\n", pct);
                } else {
                    std::printf("[LinuxPlatform] setBrightness %d%% failed\n", pct);
                }
            });
    }

    if (platform.hasDeclared("playMusic")) {
        platform.registerExternal("playMusic",
            [](const std::vector<std::string>& args) {
                const std::string path = args.empty() ? "" : args[0];
                if (path.empty()) return;
                gMusicService.requestPlay(path);
            });
    }

    if (platform.hasDeclared("stopMusic")) {
        platform.registerExternal("stopMusic",
            [](const std::vector<std::string>&) {
                gMusicService.requestStop();
            });
    }
}

} // namespace

int main() {
    lv_init();
    skiff::lvgl::createSdl3Display(800, 480, "skiff - PND home (Linux)");

    skiff::Platform platform;
    skiff::demo::PndUi ui(platform);
    registerLinuxPlatform(platform);

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

    gMusicService.shutdown();
    monitorRunning = false;
    monitor.join();
    gMusicPlayer.stop();  // 先停音频,再销毁 SDL
    skiff::lvgl::destroySdl3Display();
    return 0;
}
