// SlotHost:持有 WatchView 实例,保证跨帧地址稳定。
//
// ① watchLocal 由 attach() / take() 自动完成,不必在 AppUi::bind 里逐个登记。
// ② bind() 返回轻量 Slot 句柄,替代 unique_ptr<WatchView<T>> 成员。
// ③ body() 里写 Watch(state, builder):槽位按"所监听 State 集合 + 同 key 序号"复用,
//    与调用顺序无关——if/for 导致某次渲染少调/换序 Watch() 都不会错位;
//    一轮渲染结束时清扫本轮未命中的槽(退订 State + 从 App 注销),不留残留订阅。
//    注意:同一个 State 被条件性地 watch 多次时,同 key 内仍按出现次序对应,
//    这种场景请把条件写进 builder 内部。
//
// PageView::render / Router overlay 会推入 Guard;Watch() 必须在 Guard 内调用。
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
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
    SlotHost() : app_(0) {}

    void attach(App& app);
    void clearCaches();

    static SlotHost* current() { return currentRef(); }

    // Guard 进入时开始一轮渲染(清零各 key 的命中计数),
    // 退出时清扫本轮未命中的槽位。
    class Guard {
    public:
        explicit Guard(SlotHost& h) : prev_(SlotHost::currentRef()), host_(h) {
            SlotHost::currentRef() = &h;
            h.beginRound();
        }
        ~Guard() {
            SlotHost::currentRef() = prev_;
            host_.endRound();
        }

    private:
        SlotHost* prev_;
        SlotHost& host_;
    };

    // ③ 按"State 集合 key + 同 key 序号"取槽;首次创建 WatchView 并 watchLocal
    template <typename T, typename Fn>
    Element take(State<T>& state, Fn builder);

    template <typename A, typename B, typename Fn>
    Element take(State<A>& a, State<B>& b, Fn builder);

    template <typename A, typename B, typename C, typename Fn>
    Element take(State<A>& a, State<B>& b, State<C>& c, Fn builder);

    template <typename T, typename Fn>
    Slot bind(State<T>& state, Fn builder);

    template <typename A, typename B, typename Fn>
    Slot bind(State<A>& a, State<B>& b, Fn builder);

    template <typename A, typename B, typename C, typename Fn>
    Slot bind(State<A>& a, State<B>& b, State<C>& c, Fn builder);

private:
    struct SlotEntry {
        std::unique_ptr<Watchable> watch;
        bool touched;
    };

    static SlotHost*& currentRef() {
        static SlotHost* p = 0;
        return p;
    }

    // 槽位 key:所监听 State 地址的有序组合,如 "2:140734...,140735..."。
    // State 地址全局唯一且类型固定,天然区分不同 StateView 里同值的枚举键。
    static std::string keyOf(const void* a) {
        return std::string("1:") +
               std::to_string(reinterpret_cast<uintptr_t>(a));
    }
    static std::string keyOf(const void* a, const void* b) {
        return std::string("2:") +
               std::to_string(reinterpret_cast<uintptr_t>(a)) + "," +
               std::to_string(reinterpret_cast<uintptr_t>(b));
    }
    static std::string keyOf(const void* a, const void* b, const void* c) {
        return std::string("3:") +
               std::to_string(reinterpret_cast<uintptr_t>(a)) + "," +
               std::to_string(reinterpret_cast<uintptr_t>(b)) + "," +
               std::to_string(reinterpret_cast<uintptr_t>(c));
    }

    void beginRound() { ordinals_.clear(); }

    // 清扫本轮未命中的槽:~Watchable 会退订 State 并回调 App 注销。
    // 可能在 App::update 的 patch 循环里触发(嵌套 Watch 重建),
    // App 侧的遍历对此有移位保护。
    void endRound() {
        for (std::map<std::string, std::vector<SlotEntry> >::iterator it =
                 slots_.begin();
             it != slots_.end();) {
            std::vector<SlotEntry>& vec = it->second;
            for (size_t i = 0; i < vec.size();) {
                if (!vec[i].touched) {
                    vec.erase(vec.begin() + static_cast<std::ptrdiff_t>(i));
                } else {
                    vec[i].touched = false;
                    ++i;
                }
            }
            if (vec.empty()) it = slots_.erase(it);
            else ++it;
        }
    }

    void adopt(Watchable* w);

    App* app_;
    std::map<std::string, size_t> ordinals_;
    std::map<std::string, std::vector<SlotEntry> > slots_;
    std::vector<std::unique_ptr<Watchable> > named_;
};

} // namespace skiff
