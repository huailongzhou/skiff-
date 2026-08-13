// WatchView:把 State 与一段子树构造器绑定。
//
// 一般不直接持有 WatchView。页面 body / overlay 里写:
//   Watch(state, [](const T& v) { return Text(...); })
// SlotHost(PageView / overlay)按调用顺序复用实例,set() 只 patch 对应节点。
//
// 仍可手动构造 WatchView + App::watchLocal,供测试或自定义槽位。
// Watch() 必须在 SlotHost::Guard 内调用(PageView::render 已推入)。
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <utility>

#include "../backend.hpp"
#include "../element.hpp"
#include "../slot_host.hpp"
#include "../state.hpp"
#include "../watchable.hpp"

namespace skiff {
namespace components {

inline std::string nextWatchKey() {
    static uint64_t n = 0;
    ++n;
    return std::string("skiff.watch.") + std::to_string(n);
}

template <typename T>
class WatchView : public ElementView, public Watchable {
public:
    WatchView(State<T>& state, std::function<Element(const T&)> builder)
        : ElementView(), Watchable(), state_(state), builder_(std::move(builder)),
          lastVersion_(static_cast<uint64_t>(-1)), dirty_(true), subId_(0),
          key_(nextWatchKey()) {
        subscribe_();
    }

    WatchView(const WatchView&) = delete;
    WatchView& operator=(const WatchView&) = delete;
    WatchView& operator=(WatchView&&) = delete;

    WatchView(WatchView&& o)
        : ElementView(), Watchable(), state_(o.state_),
          builder_(std::move(o.builder_)), lastVersion_(o.lastVersion_),
          cached_(std::move(o.cached_)), dirty_(o.dirty_), subId_(0),
          key_(o.key_.empty() ? nextWatchKey() : std::move(o.key_)),
          nested_(std::move(o.nested_)) {
        o.dirty_ = false;
        if (o.subId_ != 0) {
            o.state_.unsubscribe(o.subId_);
            o.subId_ = 0;
        }
        o.runUnregister();
        subscribe_();
    }

    ~WatchView() {
        if (subId_ != 0) {
            state_.unsubscribe(subId_);
            subId_ = 0;
        }
        runUnregister();
    }

    Element build() const override {
        const uint64_t current = state_.version();
        if (dirty_ || current != lastVersion_) {
            SlotHost::Guard g(nested_);
            cached_ = builder_(state_.get());
            cached_.options.keyId = key_;
            lastVersion_ = current;
            dirty_ = false;
        }
        return cached_;
    }

    const std::string& watchKey() const override { return key_; }
    const void* stateIdentity() const override {
        return static_cast<const void*>(&state_);
    }
    bool isDirty() const override {
        return dirty_ || state_.version() != lastVersion_;
    }
    Element rebuild() override { return build(); }

    void clearCache() override {
        lastVersion_ = static_cast<uint64_t>(-1);
        dirty_ = true;
        nested_.clearCaches();
    }

    SlotHost* nestedSlots() override { return &nested_; }

    void invalidate() {
        clearCache();
        notifyInvalidator();
    }

private:
    void subscribe_() {
        subId_ = state_.subscribe([this] {
            dirty_ = true;
            lastVersion_ = static_cast<uint64_t>(-1);
            notifyInvalidator();
        });
    }

    State<T>& state_;
    std::function<Element(const T&)> builder_;
    mutable uint64_t lastVersion_;
    mutable Element cached_;
    mutable bool dirty_;
    uint64_t subId_;
    std::string key_;
    mutable SlotHost nested_;
};

} // namespace components

template <typename T, typename Fn>
inline Element SlotHost::take(State<T>& state, Fn builder) {
    if (index_ < ordered_.size()) {
        Watchable* b = ordered_[index_].get();
        ++index_;
        return b->rebuild();
    }
    components::WatchView<T>* v = new components::WatchView<T>(
        state, std::function<Element(const T&)>(std::move(builder)));
    ordered_.push_back(std::unique_ptr<Watchable>(v));
    ++index_;
    adopt(v);
    return v->build();
}

template <typename T, typename Fn>
inline Slot SlotHost::bind(State<T>& state, Fn builder) {
    components::WatchView<T>* v = new components::WatchView<T>(
        state, std::function<Element(const T&)>(std::move(builder)));
    named_.push_back(std::unique_ptr<Watchable>(v));
    adopt(v);
    return Slot(v);
}

inline void SlotHost::adopt(Watchable* b) {
    if (app_ && b) app_->watchLocal(*b);
}

template <typename T, typename Fn>
inline Element Watch(State<T>& state, Fn builder) {
    SlotHost* host = SlotHost::current();
    if (!host) {
        std::fprintf(stderr,
                     "skiff::Watch() 须在 PageView::render / overlay 内调用\n");
        std::abort();
    }
    return host->take(state, std::move(builder));
}

namespace components {

template <typename T, typename Fn>
inline Element Watch(State<T>& state, Fn builder) {
    return skiff::Watch(state, std::move(builder));
}

} // namespace components
} // namespace skiff
