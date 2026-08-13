// music_headless:无 UI 跑播放列表逻辑,证明 MusicScene 可独立运行。
//
// 运行:./build/music_headless
#include <cstdio>
#include <cstring>
#include <string>

#include "app_core/music_scene.hpp"

namespace {

class RecSink : public app::MusicSink {
public:
    RecSink() : plays(0), stops(0), vol(-1) {}

    void play(const char* path) {
        lastPlay = path ? path : "";
        ++plays;
    }
    void stop() { ++stops; }
    void setVolume(int percent) { vol = percent; }

    std::string lastPlay;
    int plays;
    int stops;
    int vol;
};

int fail(const char* msg) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

} // namespace

int main() {
    RecSink sink;
    app::MusicScene scene;
    scene.setSink(&sink);
    if (sink.vol != 70) return fail("setSink should apply default volume");

    if (scene.playing() || scene.index() != 0 || scene.progress() != 0) {
        return fail("default should be stopped at track 0");
    }

    scene.play();
    if (!scene.playing() || sink.plays != 1) return fail("play");
    if (scene.current().path == 0 ||
        std::strcmp(scene.current().path, "assets/music/sample.wav") != 0) {
        return fail("play should start first track");
    }

    scene.next();
    if (scene.index() != 1 || sink.plays != 2) return fail("next");
    if (std::strcmp(scene.current().title, "Moment of Peace") != 0) {
        return fail("next track title");
    }

    scene.setProgressFromOutput(20);
    if (scene.progress() != 20) return fail("progress from output");
    scene.prev();
    if (scene.index() != 1 || sink.plays != 3) {
        return fail("prev with progress>5 should restart current");
    }

    scene.setProgressFromOutput(0);
    scene.prev();
    if (scene.index() != 0 || sink.plays != 4) return fail("prev wraps to last-1");

    scene.playTrack(scene.track(1).path);
    if (scene.index() != 1 || !scene.playing()) return fail("playTrack");

    scene.onPlaybackEnded();
    if (scene.playing() || scene.progress() != 0) {
        return fail("ended last track without repeat should stop");
    }
    if (sink.stops < 1) return fail("ended should stop sink");

    scene.setRepeat(true);
    scene.playTrack(scene.track(1).path);
    scene.onPlaybackEnded();
    if (!scene.playing() || scene.index() != 0) {
        return fail("ended last track with repeat should wrap");
    }

    scene.setVolume(40);
    if (scene.volume() != 40 || sink.vol != 40) return fail("setVolume");

    scene.pause();
    if (scene.playing()) return fail("pause");
    {
        const int idx = scene.index();
        scene.onPlaybackEnded();
        if (scene.playing()) return fail("paused ended should stay paused");
        if (scene.index() != idx) return fail("paused ended should not skip track");
    }

    std::printf("OK: music_headless tracks=%d plays=%d\n",
                scene.trackCount(), sink.plays);
    return 0;
}
