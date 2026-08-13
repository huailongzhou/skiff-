// SlotHost:持有 BindView 实例,保证跨帧地址稳定。
//
// ① bindLocal 由 attach() / take() 自动完成,不必在 AppUi::bind 里逐个登记。
// ② bind() 返回轻量 Slot 句柄,替代 unique_ptr<BindView<T>> 成员。
// ③ body() 里写 Bind(state, builder),按调用顺序复用槽位(顺序须稳定)。
//
// PageView::render / Router overlay 会推入 Guard;Bind() 必须在 Guard 内调用。
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

#include "binding.hpp"
#include "element.hpp"
#include "state.hpp"

namespace skiff {

class App;

class Slot {
public:
    Slot() : b_(0) {}
    explicit Slot(Binding* b) : b_(b) {}

    Element build() const { return b_ ? b_->rebuild() : Element(); }
    operator Element() const { return build(); }
    Binding* binding() const { return b_; }

private:
    Binding* b_;
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

    // ③ 按调用顺序取槽;首次创建 BindView 并 bindLocal
    template <typename T, typename Fn>
    Element take(State<T>& state, Fn builder);

    template <typename T, typename Fn>
    Slot bind(State<T>& state, Fn builder);

private:
    static SlotHost*& currentRef() {
        static SlotHost* p = 0;
        return p;
    }

    void adopt(Binding* b);

    App* app_;
    size_t index_;
    std::vector<std::unique_ptr<Binding> > ordered_;
    std::vector<std::unique_ptr<Binding> > named_;
};

} // namespace skiff
