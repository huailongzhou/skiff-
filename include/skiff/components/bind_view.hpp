// BindView:把 State 与一段子树构造器绑定,按 State 版本缓存 build() 结果。
//
// 与 MemoView 的区别:BindView 自动追踪 State::version(),set() 即失效,
// 不需要用户手动维护 key。
//
// 重要:ElementView 对象通常是临时生成的,BindView 实例必须在 body() lambda
// 外部保持存活、跨帧复用同一个实例,缓存才会生效。如果像普通 Element 一样
// 在 body() 里临时构造 skiff::Bind(...).build(),每帧都是新实例,缓存永远不会命中。
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
#pragma once

#include <cstdint>
#include <functional>

#include "../element.hpp"
#include "../state.hpp"

namespace skiff {
namespace components {

template <typename T>
class BindView : public ElementView {
public:
    BindView(State<T>& state, std::function<Element(const T&)> builder)
        : ElementView(), state_(state), builder_(std::move(builder)),
          lastVersion_(static_cast<uint64_t>(-1)) {}

    // State 版本未变化时直接返回缓存的子树,否则重新构造并缓存。
    Element build() const override {
        const uint64_t current = state_.version();
        if (current != lastVersion_) {
            cached_ = builder_(state_.get());
            lastVersion_ = current;
        }
        return cached_;
    }

    // 强制下次 build() 重新构造。
    void invalidate() { lastVersion_ = static_cast<uint64_t>(-1); }

private:
    State<T>& state_;
    std::function<Element(const T&)> builder_;
    mutable uint64_t lastVersion_;
    mutable Element cached_;
};

// 工厂函数,省去显式模板参数。
template <typename T>
inline BindView<T> Bind(State<T>& state,
                        std::function<Element(const T&)> builder) {
    return BindView<T>(state, std::move(builder));
}

} // namespace components
} // namespace skiff
