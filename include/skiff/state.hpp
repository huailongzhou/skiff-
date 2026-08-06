// 类似 SwiftUI 的 @State:持有页面状态,值变化时通知框架重建视图树。
#pragma once

#include <cstdint>
#include <functional>
#include <utility>

namespace skiff {

template <typename T>
class State {
public:
    explicit State(const T& initial) : value_(initial), version_(0) {}

    const T& get() const { return value_; }

    // 每次 set() 自增,用于 BindView 等细粒度缓存判断
    uint64_t version() const { return version_; }

    void set(const T& value) {
        value_ = value;
        ++version_;
        if (onChange_) onChange_();
    }

    // 框架内部使用:注册失效回调(见 App::bind)
    void setOnChange(std::function<void()> cb) { onChange_ = std::move(cb); }

private:
    T value_;
    uint64_t version_;
    std::function<void()> onChange_;
};

} // namespace skiff
