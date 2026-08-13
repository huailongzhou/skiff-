// PND 平台适配:把应用核/UI 的设备请求接到 skiff::Platform。
#pragma once

#include "app_core/music_scene.hpp"
#include "skiff/platform.hpp"

namespace pnd {

class PlatformMusicSink : public app::MusicSink {
public:
    explicit PlatformMusicSink(skiff::Platform& platform);
    void play(const char* path);
    void stop();
    void setVolume(int percent);

private:
    skiff::Platform& platform_;
};

class PlatformDisplay {
public:
    explicit PlatformDisplay(skiff::Platform& platform);
    void setBrightness(int percent);

private:
    skiff::Platform& platform_;
};

// 声明 PND 需要的平台能力,并把播放器事件接到 MusicScene。
void bindPlatform(skiff::Platform& platform, app::MusicScene& music);

} // namespace pnd
