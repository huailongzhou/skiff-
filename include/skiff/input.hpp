// input:后端无关的输入/手势管理。
//
// 后端(如 SDL3)把原始指针事件转换为 skiff::input::pointerDown/Up/Move 喂进来;
// 页面代码只注册高层手势(如 onSwipeDown),不感知具体后端。
#pragma once

#include <functional>

namespace skiff {
namespace input {

struct Point {
    float x;
    float y;
};

using GestureHandler = std::function<void()>;

// 注册从屏幕顶部向下滑动的手势。
// topZone: 顶部热区高度(像素),按下时必须落在该区域内。
// threshold: 需要向下滑动的最小距离(像素)。
// handler: 手势触发时调用。
inline void onSwipeDown(float topZone, float threshold, GestureHandler handler);

// 后端调用:指针按下/抬起/移动,坐标为屏幕像素。
inline void pointerDown(const Point& p);
inline void pointerUp(const Point& p);
inline void pointerMove(const Point& p);

namespace detail {

struct SwipeState {
    bool tracking;
    float startY;
    float topZone;
    float threshold;
    GestureHandler handler;

    SwipeState()
        : tracking(false), startY(0.0f),
          topZone(0.0f), threshold(0.0f) {}
};

inline SwipeState& swipeState() {
    static SwipeState s;
    return s;
}

} // namespace detail

inline void onSwipeDown(float topZone, float threshold, GestureHandler handler) {
    detail::swipeState().topZone = topZone;
    detail::swipeState().threshold = threshold;
    detail::swipeState().handler = std::move(handler);
}

inline void pointerDown(const Point& p) {
    detail::SwipeState& s = detail::swipeState();
    if (p.y < s.topZone) {
        s.tracking = true;
        s.startY = p.y;
    }
}

inline void pointerUp(const Point& p) {
    detail::SwipeState& s = detail::swipeState();
    if (s.tracking && (p.y - s.startY) > s.threshold) {
        if (s.handler) s.handler();
    }
    s.tracking = false;
}

inline void pointerMove(const Point& p) {
    detail::SwipeState& s = detail::swipeState();
    // 如果手指向上滑出热区,取消本次跟踪
    if (s.tracking && p.y < s.startY) {
        s.tracking = false;
    }
}

} // namespace input
} // namespace skiff
