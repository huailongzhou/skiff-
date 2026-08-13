// BindView:把 State 与一段子树构造器绑定。
//
// 默认行为(与 MemoView 类似):按 State::version() 缓存 build() 结果。
// 再调用 App::bindLocal(view) 后,State::set() 只 patch 这一段已挂载子树,
// 不再重跑整页 body()。未 bindLocal 的 BindView 仍只作缓存,整页失效照旧。
//
// 重要:ElementView 对象通常是临时生成的,BindView 实例必须在 body() lambda
// 外部保持存活、跨帧复用同一个实例。如果像普通 Element 一样
// 在 body() 里临时构造 skiff::Bind(...).build(),每帧都是新实例,缓存与局部
// patch 都不会生效。
//
// 用法:
//   skiff::State<int> count(0);
//   skiff::components::BindView<int> counterBind(
//       count, [](int c) { return skiff::Text(std::to_string(c)); });
//
//   auto body = [&]() -> skiff::Element {
//       return skiff::VStack({
//           counterBind.build(),
//           skiff::Text("静态文本"),
//       });
//   };
//   app.bind(count);
//   app.bindLocal(counterBind);  // 只影响一段子树时使用
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "../binding.hpp"
#include "../element.hpp"
#include "../state.hpp"

namespace skiff {
namespace components {

inline std::string nextBindKey() {
    static uint64_t n = 0;
    ++n;
    return std::string("skiff.bind.") + std::to_string(n);
}

template <typename T>
class BindView : public ElementView, public Binding {
public:
    BindView(State<T>& state, std::function<Element(const T&)> builder)
        : ElementView(), Binding(), state_(state), builder_(std::move(builder)),
          lastVersion_(static_cast<uint64_t>(-1)), dirty_(true), subId_(0),
          key_(nextBindKey()) {
        subscribe_();
    }

    BindView(const BindView&) = delete;
    BindView& operator=(const BindView&) = delete;
    BindView& operator=(BindView&&) = delete;

    BindView(BindView&& o)
        : ElementView(), Binding(), state_(o.state_),
          builder_(std::move(o.builder_)), lastVersion_(o.lastVersion_),
          cached_(std::move(o.cached_)), dirty_(o.dirty_), subId_(0),
          key_(o.key_.empty() ? nextBindKey() : std::move(o.key_)) {
        o.dirty_ = false;
        if (o.subId_ != 0) {
            o.state_.unsubscribe(o.subId_);
            o.subId_ = 0;
        }
        o.runUnregister();
        subscribe_();
    }

    ~BindView() {
        if (subId_ != 0) {
            state_.unsubscribe(subId_);
            subId_ = 0;
        }
        runUnregister();
    }

    // State 版本未变化时直接返回缓存的子树,否则重新构造并缓存。
    // 根节点写入稳定 key,供 Backend::patch 定位。
    Element build() const override {
        const uint64_t current = state_.version();
        if (dirty_ || current != lastVersion_) {
            cached_ = builder_(state_.get());
            cached_.options.keyId = key_;
            lastVersion_ = current;
            dirty_ = false;
        }
        return cached_;
    }

    const std::string& bindingKey() const override { return key_; }
    const void* stateIdentity() const override {
        return static_cast<const void*>(&state_);
    }
    bool isDirty() const override {
        return dirty_ || state_.version() != lastVersion_;
    }
    Element rebuild() override { return build(); }

    // 强制下次 build() 重新构造。
    void invalidate() {
        lastVersion_ = static_cast<uint64_t>(-1);
        dirty_ = true;
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
};

// 工厂函数,省去显式模板参数。
template <typename T>
inline BindView<T> Bind(State<T>& state,
                        std::function<Element(const T&)> builder) {
    return BindView<T>(state, std::move(builder));
}

} // namespace components
} // namespace skiff
