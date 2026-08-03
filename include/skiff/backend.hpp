// Backend:把 Element 树挂载成原生控件树的后端接口。
//
// LVGL 只是其中一种实现;以后做 PC 端预览后端(SDL / Qt / imgui ...),
// 实现同一个接口即可,页面代码不用改。
#pragma once

#include <functional>

#include "element.hpp"
#include "state.hpp"

namespace skiff {

class Backend {
public:
    virtual ~Backend() {}

    // 用一棵新的 Element 树重建整个原生控件树。
    // 实现方负责旧树的释放。
    virtual void mount(const Element& root) = 0;
};

class App {
public:
    App(Backend& backend, std::function<Element()> body)
        : backend_(backend), body_(std::move(body)), dirty_(true) {}

    // 绑定一个状态:它变化时标记界面待刷新。可绑定多个。
    template <typename T>
    void bind(State<T>& s) {
        s.setOnChange([this] { invalidate(); });
    }

    // 首次挂载。
    void start() { update(); }

    // 标记待刷新。真正的重建延迟到 update(),
    // 避免在后端的事件回调里边派发事件边删控件。
    void invalidate() { dirty_ = true; }

    // 主循环里定期调用;有失效标记时重新执行 body() 并重建视图树。
    void update() {
        if (!dirty_) return;
        dirty_ = false;
        backend_.mount(body_());
    }

private:
    Backend& backend_;
    std::function<Element()> body_;
    bool dirty_;
};

} // namespace skiff
