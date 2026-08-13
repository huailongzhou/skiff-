// SlotHost:持有 WatchView 实例,保证跨帧地址稳定。
//
// ① watchLocal 由 attach() / take() 自动完成,不必在 AppUi::bind 里逐个登记。
// ② bind() 返回轻量 Slot 句柄,替代 unique_ptr<WatchView<T>> 成员。
// ③ body() 里写 Watch(state, builder),按调用顺序复用槽位(顺序须稳定)。
//
// PageView::render / Router overlay 会推入 Guard;Watch() 必须在 Guard 内调用。
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

#include "element.hpp"
#include "state.hpp"
#include "watchable.hpp"

namespace skiff {

class App;

class Slot {
public:
    Slot() : w_(0) {}
    explicit Slot(Watchable* w) : w_(w) {}

    Element build() const { return w_ ? w_->rebuild() : Element(); }
    operator Element() const { return build(); }
    Watchable* watchable() const { return w_; }

private:
    Watchable* w_;
};

class SlotHost {
public:
    SlotHost() : app_(0), index_(0) {}

    void attach(App& app);
    void clearCaches();

    static SlotHost* current() { return currentRef(); }

    class Guard {
    public:
        explicit Guard(SlotHost& h) : prev_(SlotHost::currentRef()) {
            SlotHost::currentRef() = &h;
            h.index_ = 0;
        }
        ~Guard() { SlotHost::currentRef() = prev_; }

    private:
        SlotHost* prev_;
    };

    // ③ 按调用顺序取槽;首次创建 WatchView 并 watchLocal
    template <typename T, typename Fn>
    Element take(State<T>& state, Fn builder);

    template <typename T, typename Fn>
    Slot bind(State<T>& state, Fn builder);

private:
    static SlotHost*& currentRef() {
        static SlotHost* p = 0;
        return p;
    }

    void adopt(Watchable* w);

    App* app_;
    size_t index_;
    std::vector<std::unique_ptr<Watchable> > ordered_;
    std::vector<std::unique_ptr<Watchable> > named_;
};

} // namespace skiff
