// MemoView:按 key 缓存 build() 结果的 ElementView。
//
// 用法注意:ElementView 对象通常是临时生成的,所以 MemoView 实例必须
// 在 body() lambda 外部保持存活,跨帧复用同一个实例,缓存才会生效。
//
// 示例:
//   MemoView clockView("clock", [&time]() { return skiff::Text(time); });
//   auto body = [&]() -> skiff::Element {
//       return skiff::VStack({ clockView.build() });
//   };
#pragma once

#include <functional>
#include <string>

#include "../element.hpp"

namespace skiff {
namespace components {

class MemoView : public ElementView {
public:
    MemoView(const std::string& key, std::function<Element()> builder)
        : ElementView(), key_(key), builder_(std::move(builder)) {}

    MemoView(const char* key, std::function<Element()> builder)
        : ElementView(), key_(key), builder_(std::move(builder)) {}

    // 当 key 未变化时直接返回缓存的 Element,否则重新 build 并缓存。
    Element build() const override {
        if (key_ != cachedKey_) {
            cached_ = builder_();
            cachedKey_ = key_;
        }
        return cached_;
    }

    // 手动失效缓存,下次 build() 会重新生成。
    void invalidate() { cachedKey_.clear(); }

    // 更换 key 与 builder。
    void reset(const std::string& key, std::function<Element()> builder) {
        key_ = key;
        builder_ = std::move(builder);
        cachedKey_.clear();
    }

private:
    std::string key_;
    std::function<Element()> builder_;
    mutable std::string cachedKey_;
    mutable Element cached_;
};

} // namespace components
} // namespace skiff
