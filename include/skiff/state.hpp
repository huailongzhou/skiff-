// 类似 SwiftUI 的 @State:持有页面状态。
// set() 发布给所有订阅者:未 watchLocal 的走整页失效,已 watchLocal 的 WatchView 只 patch 对应节点。
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace skiff {

template <typename T>
class State {
public:
    explicit State(const T& initial)
        : value_(initial), version_(0), nextSubId_(0), onChangeId_(0) {}

    const T& get() const { return value_; }

    // 每次 set() 自增,用于 WatchView 等细粒度缓存判断
    uint64_t version() const { return version_; }

    void set(const T& value) {
        value_ = value;
        ++version_;
        notify();
    }

    // 值未变则不 bump、不 notify。场景投影到 UI State 时用这个。
    void setIfChanged(const T& value) {
        if (value_ == value) return;
        set(value);
    }

    // 注册订阅者,返回 id,供 unsubscribe 使用。可同时存在多个订阅者。
    uint64_t subscribe(std::function<void()> cb) {
        const uint64_t id = ++nextSubId_;
        SubscribersEntry e;
        e.id = id;
        e.cb = std::move(cb);
        subs_.push_back(e);
        return id;
    }

    void unsubscribe(uint64_t id) {
        if (id == 0) return;
        for (size_t i = 0; i < subs_.size(); ++i) {
            if (subs_[i].id == id) {
                subs_.erase(subs_.begin() + static_cast<std::ptrdiff_t>(i));
                if (onChangeId_ == id) onChangeId_ = 0;
                return;
            }
        }
    }

    // 兼容旧 API:替换此前 setOnChange 登记的那一个订阅,不影响其它 subscribe()
    void setOnChange(std::function<void()> cb) {
        if (onChangeId_ != 0) {
            unsubscribe(onChangeId_);
            onChangeId_ = 0;
        }
        if (cb) onChangeId_ = subscribe(std::move(cb));
    }

private:
    struct SubscribersEntry {
        uint64_t id;
        std::function<void()> cb;
    };

    void notify() {
        // 拷贝后再调用,允许回调里 subscribe/unsubscribe
        std::vector<std::function<void()> > cbs;
        cbs.reserve(subs_.size());
        for (size_t i = 0; i < subs_.size(); ++i) cbs.push_back(subs_[i].cb);
        for (size_t i = 0; i < cbs.size(); ++i) {
            if (cbs[i]) cbs[i]();
        }
    }

    T value_;
    uint64_t version_;
    uint64_t nextSubId_;
    uint64_t onChangeId_;
    std::vector<SubscribersEntry> subs_;
};

} // namespace skiff
