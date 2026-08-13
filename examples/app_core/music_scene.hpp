// MusicScene:播放列表与播放状态(无 UI)。真正出声走 MusicSink,由平台层实现。
#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "scene.hpp"

namespace app {

struct TrackInfo {
    const char* path;
    const char* title;
    const char* artist;
    int durationSec;
    uint32_t artColor;
};

class MusicSink {
public:
    virtual ~MusicSink() {}
    virtual void play(const char* path) = 0;
    virtual void stop() = 0;
    virtual void setVolume(int percent) { (void)percent; }
};

class MusicScene : public Scene {
public:
    MusicScene()
        : sink_(0), index_(0), progress_(0), volume_(70), playing_(false),
          shuffle_(false), repeat_(false) {}

    MusicScene(const MusicScene&) = delete;
    MusicScene& operator=(const MusicScene&) = delete;

    const char* name() const { return "music"; }
    void tick(float) {}

    void setSink(MusicSink* sink) {
        sink_ = sink;
        if (sink_) sink_->setVolume(volume_);
    }

    int trackCount() const { return kTrackCount; }

    const TrackInfo& track(int i) const {
        if (i < 0 || i >= kTrackCount) return tracks_()[0];
        return tracks_()[i];
    }

    const TrackInfo& current() const { return tracks_()[index_]; }
    int index() const { return index_; }
    int progress() const { return progress_; }
    int volume() const { return volume_; }
    bool playing() const { return playing_; }
    bool shuffle() const { return shuffle_; }
    bool repeat() const { return repeat_; }

    void play() { playIndex_(index_); }

    void pause() {
        if (!playing_) return;
        playing_ = false;
        if (sink_) sink_->stop();
        notify();
    }

    void togglePlay() {
        if (playing_) pause();
        else play();
    }

    void playTrack(const char* path) {
        const int i = indexOf_(path);
        if (i < 0) return;
        playIndex_(i);
    }

    void next() {
        playIndex_(shuffle_ ? shuffledOther_(index_)
                            : (index_ + 1) % kTrackCount);
    }

    void prev() {
        if (progress_ > 5) {
            playIndex_(index_);
            return;
        }
        const int i = (index_ - 1 + kTrackCount) % kTrackCount;
        playIndex_(i);
    }

    void setShuffle(bool on) {
        if (shuffle_ == on) return;
        shuffle_ = on;
        notify();
    }

    void toggleShuffle() { setShuffle(!shuffle_); }

    void setRepeat(bool on) {
        if (repeat_ == on) return;
        repeat_ = on;
        notify();
    }

    void toggleRepeat() { setRepeat(!repeat_); }

    void setVolume(int percent) {
        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;
        if (volume_ == percent) return;
        volume_ = percent;
        if (sink_) sink_->setVolume(volume_);
        notify();
    }

    // 播放器上报的进度(0-100)。不回写 sink。
    void setProgressFromOutput(int percent) {
        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;
        if (progress_ == percent) return;
        progress_ = percent;
        notify();
    }

    // UI 拖动进度。当前各平台播放器不支持 seek,只更新显示,随后仍以播放器进度为准。
    void seek(int percent) { setProgressFromOutput(percent); }

    // 当前曲播完:随机/顺序切下一首;列表末尾且未循环则停止。
    // 用户暂停也会让播放器停,此时 playing_ 已是 false,不能当成播完切歌。
    void onPlaybackEnded() {
        if (!playing_) return;
        if (shuffle_) {
            playIndex_(shuffledOther_(index_));
            return;
        }
        const int n = index_ + 1;
        if (n >= kTrackCount) {
            if (repeat_) playIndex_(0);
            else stopAtEnd_();
            return;
        }
        playIndex_(n);
    }

private:
    static const TrackInfo* tracks_() {
        static const TrackInfo k[2] = {
            {"assets/music/sample.wav", "Sample", "Demo", 32, 0xD84315},
            {"assets/music/mickeyscat-moment-of-peace-mickeyscat-554494.mp3",
             "Moment of Peace", "Mickeyscat", 214, 0x1B4F72},
        };
        return k;
    }

    int indexOf_(const char* path) const {
        if (!path) return -1;
        for (int i = 0; i < kTrackCount; ++i) {
            if (std::strcmp(tracks_()[i].path, path) == 0) return i;
        }
        return -1;
    }

    int shuffledOther_(int current) const {
        if (kTrackCount <= 1) return 0;
        int n = std::rand() % (kTrackCount - 1);
        if (n >= current) ++n;
        return n;
    }

    void playIndex_(int i) {
        if (i < 0 || i >= kTrackCount) return;
        index_ = i;
        progress_ = 0;
        playing_ = true;
        if (sink_) sink_->play(tracks_()[i].path);
        notify();
    }

    void stopAtEnd_() {
        playing_ = false;
        progress_ = 0;
        if (sink_) sink_->stop();
        notify();
    }

    static const int kTrackCount = 2;

    MusicSink* sink_;
    int index_;
    int progress_;
    int volume_;
    bool playing_;
    bool shuffle_;
    bool repeat_;
};

} // namespace app
