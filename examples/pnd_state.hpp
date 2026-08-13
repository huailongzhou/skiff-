// PND UI 状态键:每个 StateView 一份枚举,从 0 起,互不共享。
// 框架 StateView 只认 int;路由名仍是字符串。
#pragma once

namespace pnd {
namespace g {  // AppUi 全局 StateView
enum {
    menuExpanded = 0,
    musicPlaying,
    brightness,
    musicProgress,
    mediaCategory,
    currentTrack,
    locale
};
}
namespace music {  // 音乐页
enum { shuffle = 0, repeat, volume };
}
namespace phys {  // 物理页
enum { frame = 0, paused, shape };
}
namespace settings {  // 设置页
enum { tab = 0 };
}
} // namespace pnd
