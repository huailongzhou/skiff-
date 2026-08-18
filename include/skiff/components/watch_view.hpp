// WatchView:把 State 与一段子树构造器绑定。
//
// 一般不直接持有 WatchView。页面 body / overlay 里写:
//   Watch(state, [](const T& v) { return Text(...); })
//   Watch(a, b, [](const A& x, const B& y) { return ...; })  // 任一变化即重建
// SlotHost(PageView / overlay)按调用顺序复用实例,set() 只 patch 对应节点。
//
// 仍可手动构造 WatchView + App::watchLocal,供测试或自定义槽位。
// Watch() 必须在 SlotHost::Guard 内调用(PageView::render 已推入)。
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
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

// 双 State:任一 version 变化即重建。两个 State 都标成本地失效。
template <typename A, typename B>
class WatchView2 : public ElementView, public Watchable {
public:
    WatchView2(State<A>& a, State<B>& b,
               std::function<Element(const A&, const B&)> builder)
        : ElementView(), Watchable(), a_(a), b_(b), builder_(std::move(builder)),
          lastA_(static_cast<uint64_t>(-1)), lastB_(static_cast<uint64_t>(-1)),
          dirty_(true), subA_(0), subB_(0), key_(nextWatchKey()) {
        subscribe_();
    }

    WatchView2(const WatchView2&) = delete;
    WatchView2& operator=(const WatchView2&) = delete;
    WatchView2& operator=(WatchView2&&) = delete;

    WatchView2(WatchView2&& o)
        : ElementView(), Watchable(), a_(o.a_), b_(o.b_),
          builder_(std::move(o.builder_)), lastA_(o.lastA_), lastB_(o.lastB_),
          cached_(std::move(o.cached_)), dirty_(o.dirty_), subA_(0), subB_(0),
          key_(o.key_.empty() ? nextWatchKey() : std::move(o.key_)),
          nested_(std::move(o.nested_)) {
        o.dirty_ = false;
        if (o.subA_ != 0) {
            o.a_.unsubscribe(o.subA_);
            o.subA_ = 0;
        }
        if (o.subB_ != 0) {
            o.b_.unsubscribe(o.subB_);
            o.subB_ = 0;
        }
        o.runUnregister();
        subscribe_();
    }

    ~WatchView2() {
        if (subA_ != 0) {
            a_.unsubscribe(subA_);
            subA_ = 0;
        }
        if (subB_ != 0) {
            b_.unsubscribe(subB_);
            subB_ = 0;
        }
        runUnregister();
    }

    Element build() const override {
        const uint64_t va = a_.version();
        const uint64_t vb = b_.version();
        if (dirty_ || va != lastA_ || vb != lastB_) {
            SlotHost::Guard g(nested_);
            cached_ = builder_(a_.get(), b_.get());
            cached_.options.keyId = key_;
            lastA_ = va;
            lastB_ = vb;
            dirty_ = false;
        }
        return cached_;
    }

    const std::string& watchKey() const override { return key_; }
    const void* stateIdentity() const override {
        return static_cast<const void*>(&a_);
    }
    size_t watchedStateCount() const override { return 2; }
    const void* watchedStateAt(size_t i) const override {
        return i == 0 ? static_cast<const void*>(&a_)
                      : static_cast<const void*>(&b_);
    }
    bool isDirty() const override {
        return dirty_ || a_.version() != lastA_ || b_.version() != lastB_;
    }
    Element rebuild() override { return build(); }

    void clearCache() override {
        lastA_ = static_cast<uint64_t>(-1);
        lastB_ = static_cast<uint64_t>(-1);
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
        std::function<void()> on = [this] {
            dirty_ = true;
            lastA_ = static_cast<uint64_t>(-1);
            lastB_ = static_cast<uint64_t>(-1);
            notifyInvalidator();
        };
        subA_ = a_.subscribe(on);
        subB_ = b_.subscribe(on);
    }

    State<A>& a_;
    State<B>& b_;
    std::function<Element(const A&, const B&)> builder_;
    mutable uint64_t lastA_;
    mutable uint64_t lastB_;
    mutable Element cached_;
    mutable bool dirty_;
    uint64_t subA_;
    uint64_t subB_;
    std::string key_;
    mutable SlotHost nested_;
};

// 三 State:任一 version 变化即重建。
template <typename A, typename B, typename C>
class WatchView3 : public ElementView, public Watchable {
public:
    WatchView3(State<A>& a, State<B>& b, State<C>& c,
               std::function<Element(const A&, const B&, const C&)> builder)
        : ElementView(), Watchable(), a_(a), b_(b), c_(c),
          builder_(std::move(builder)), lastA_(static_cast<uint64_t>(-1)),
          lastB_(static_cast<uint64_t>(-1)), lastC_(static_cast<uint64_t>(-1)),
          dirty_(true), subA_(0), subB_(0), subC_(0), key_(nextWatchKey()) {
        subscribe_();
    }

    WatchView3(const WatchView3&) = delete;
    WatchView3& operator=(const WatchView3&) = delete;
    WatchView3& operator=(WatchView3&&) = delete;

    WatchView3(WatchView3&& o)
        : ElementView(), Watchable(), a_(o.a_), b_(o.b_), c_(o.c_),
          builder_(std::move(o.builder_)), lastA_(o.lastA_), lastB_(o.lastB_),
          lastC_(o.lastC_), cached_(std::move(o.cached_)), dirty_(o.dirty_),
          subA_(0), subB_(0), subC_(0),
          key_(o.key_.empty() ? nextWatchKey() : std::move(o.key_)),
          nested_(std::move(o.nested_)) {
        o.dirty_ = false;
        if (o.subA_ != 0) {
            o.a_.unsubscribe(o.subA_);
            o.subA_ = 0;
        }
        if (o.subB_ != 0) {
            o.b_.unsubscribe(o.subB_);
            o.subB_ = 0;
        }
        if (o.subC_ != 0) {
            o.c_.unsubscribe(o.subC_);
            o.subC_ = 0;
        }
        o.runUnregister();
        subscribe_();
    }

    ~WatchView3() {
        if (subA_ != 0) {
            a_.unsubscribe(subA_);
            subA_ = 0;
        }
        if (subB_ != 0) {
            b_.unsubscribe(subB_);
            subB_ = 0;
        }
        if (subC_ != 0) {
            c_.unsubscribe(subC_);
            subC_ = 0;
        }
        runUnregister();
    }

    Element build() const override {
        const uint64_t va = a_.version();
        const uint64_t vb = b_.version();
        const uint64_t vc = c_.version();
        if (dirty_ || va != lastA_ || vb != lastB_ || vc != lastC_) {
            SlotHost::Guard g(nested_);
            cached_ = builder_(a_.get(), b_.get(), c_.get());
            cached_.options.keyId = key_;
            lastA_ = va;
            lastB_ = vb;
            lastC_ = vc;
            dirty_ = false;
        }
        return cached_;
    }

    const std::string& watchKey() const override { return key_; }
    const void* stateIdentity() const override {
        return static_cast<const void*>(&a_);
    }
    size_t watchedStateCount() const override { return 3; }
    const void* watchedStateAt(size_t i) const override {
        if (i == 0) return static_cast<const void*>(&a_);
        if (i == 1) return static_cast<const void*>(&b_);
        return static_cast<const void*>(&c_);
    }
    bool isDirty() const override {
        return dirty_ || a_.version() != lastA_ || b_.version() != lastB_ ||
               c_.version() != lastC_;
    }
    Element rebuild() override { return build(); }

    void clearCache() override {
        lastA_ = static_cast<uint64_t>(-1);
        lastB_ = static_cast<uint64_t>(-1);
        lastC_ = static_cast<uint64_t>(-1);
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
        std::function<void()> on = [this] {
            dirty_ = true;
            lastA_ = static_cast<uint64_t>(-1);
            lastB_ = static_cast<uint64_t>(-1);
            lastC_ = static_cast<uint64_t>(-1);
            notifyInvalidator();
        };
        subA_ = a_.subscribe(on);
        subB_ = b_.subscribe(on);
        subC_ = c_.subscribe(on);
    }

    State<A>& a_;
    State<B>& b_;
    State<C>& c_;
    std::function<Element(const A&, const B&, const C&)> builder_;
    mutable uint64_t lastA_;
    mutable uint64_t lastB_;
    mutable uint64_t lastC_;
    mutable Element cached_;
    mutable bool dirty_;
    uint64_t subA_;
    uint64_t subB_;
    uint64_t subC_;
    std::string key_;
    mutable SlotHost nested_;
};

} // namespace components

inline SlotHost& requireWatchHost() {
    SlotHost* host = SlotHost::current();
    if (!host) {
        std::fprintf(stderr,
                     "skiff::Watch() 须在 PageView::render / overlay 内调用\n");
        std::abort();
    }
    return *host;
}

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

template <typename A, typename B, typename Fn>
inline Element SlotHost::take(State<A>& a, State<B>& b, Fn builder) {
    if (index_ < ordered_.size()) {
        Watchable* w = ordered_[index_].get();
        ++index_;
        return w->rebuild();
    }
    components::WatchView2<A, B>* v = new components::WatchView2<A, B>(
        a, b, std::function<Element(const A&, const B&)>(std::move(builder)));
    ordered_.push_back(std::unique_ptr<Watchable>(v));
    ++index_;
    adopt(v);
    return v->build();
}

template <typename A, typename B, typename C, typename Fn>
inline Element SlotHost::take(State<A>& a, State<B>& b, State<C>& c, Fn builder) {
    if (index_ < ordered_.size()) {
        Watchable* w = ordered_[index_].get();
        ++index_;
        return w->rebuild();
    }
    components::WatchView3<A, B, C>* v = new components::WatchView3<A, B, C>(
        a, b, c,
        std::function<Element(const A&, const B&, const C&)>(std::move(builder)));
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

template <typename A, typename B, typename Fn>
inline Slot SlotHost::bind(State<A>& a, State<B>& b, Fn builder) {
    components::WatchView2<A, B>* v = new components::WatchView2<A, B>(
        a, b, std::function<Element(const A&, const B&)>(std::move(builder)));
    named_.push_back(std::unique_ptr<Watchable>(v));
    adopt(v);
    return Slot(v);
}

template <typename A, typename B, typename C, typename Fn>
inline Slot SlotHost::bind(State<A>& a, State<B>& b, State<C>& c, Fn builder) {
    components::WatchView3<A, B, C>* v = new components::WatchView3<A, B, C>(
        a, b, c,
        std::function<Element(const A&, const B&, const C&)>(std::move(builder)));
    named_.push_back(std::unique_ptr<Watchable>(v));
    adopt(v);
    return Slot(v);
}

inline void SlotHost::adopt(Watchable* b) {
    if (app_ && b) app_->watchLocal(*b);
}

template <typename T, typename Fn>
inline Element Watch(State<T>& state, Fn builder) {
    return requireWatchHost().take(state, std::move(builder));
}

template <typename A, typename B, typename Fn>
inline Element Watch(State<A>& a, State<B>& b, Fn builder) {
    return requireWatchHost().take(a, b, std::move(builder));
}

template <typename A, typename B, typename C, typename Fn>
inline Element Watch(State<A>& a, State<B>& b, State<C>& c, Fn builder) {
    return requireWatchHost().take(a, b, c, std::move(builder));
}

namespace components {

template <typename T, typename Fn>
inline Element Watch(State<T>& state, Fn builder) {
    return skiff::Watch(state, std::move(builder));
}

template <typename A, typename B, typename Fn>
inline Element Watch(State<A>& a, State<B>& b, Fn builder) {
    return skiff::Watch(a, b, std::move(builder));
}

template <typename A, typename B, typename C, typename Fn>
inline Element Watch(State<A>& a, State<B>& b, State<C>& c, Fn builder) {
    return skiff::Watch(a, b, c, std::move(builder));
}

} // namespace components
} // namespace skiff
