// PND 平台适配:设备请求 → Platform 延后调用(勿在 LVGL 点击回调里同步开设备)。
#include "pnd_platform.hpp"

#include <cstdlib>
#include <string>
#include <vector>

namespace pnd {

PlatformMusicSink::PlatformMusicSink(skiff::Platform& platform)
    : platform_(platform) {}

void PlatformMusicSink::play(const char* path) {
    platform_.invokeLater("stopMusic", {});
    if (path) platform_.invokeLater("playMusic", {std::string(path)});
}

void PlatformMusicSink::stop() { platform_.invokeLater("stopMusic", {}); }

void PlatformMusicSink::setVolume(int percent) {
    platform_.invokeLater("setVolume", {std::to_string(percent)});
}

PlatformDisplay::PlatformDisplay(skiff::Platform& platform)
    : platform_(platform) {}

void PlatformDisplay::setBrightness(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    platform_.invokeLater("setBrightness", {std::to_string(percent)});
}

void bindPlatform(skiff::Platform& platform, app::MusicScene& music) {
    platform.declare("setBrightness");
    platform.declare("setVolume");
    platform.declare("playMusic");
    platform.declare("stopMusic");
    platform.declare("openFile");
    platform.on("musicProgress", [&music](const std::vector<std::string>& args) {
        if (!args.empty()) {
            music.setProgressFromOutput(std::atoi(args[0].c_str()));
        }
    });
    platform.on("musicEnded", [&music](const std::vector<std::string>&) {
        music.onPlaybackEnded();
    });
}

} // namespace pnd
