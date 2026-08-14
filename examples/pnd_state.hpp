// PND UI 状态键:每个 StateView 一份枚举,从 0 起,互不共享。
// 框架 StateView 只认 int;路由名仍是字符串。
//
// 音乐/物理里与 Scene 同名的键是投影:只由 onChange → setIfChanged 写入,
// 页面点击走 scene 命令,不要当主数据源 set()。
#pragma once

namespace pnd {
namespace g {  // AppUi 全局 StateView(纯 UI,无对应 Scene)
enum {
    menuExpanded = 0,
    brightness,
    mediaCategory,
    locale
};
}
namespace music {  // 音乐页:Watch 用的 Scene 投影
enum {
    shuffle = 0,
    repeat,
    volume,
    playing,
    progress,
    currentTrack
};
}
namespace phys {  // 物理页:Watch 用的 Scene 投影
enum { frame = 0, paused, shape };
}
namespace settings {  // 设置页
enum { tab = 0 };
}
} // namespace pnd
