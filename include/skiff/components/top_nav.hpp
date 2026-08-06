// TopNav:页面顶部导航条(纯 DSL 组合,后端无关)。
// TopNavView 继承 ElementView,可通过 .build() 展开成通用 Element 树。
#pragma once

#include <initializer_list>
#include <string>
#include <vector>

#include "../element.hpp"
#include "router.hpp"

namespace skiff {
namespace components {

class TopNavView : public ElementView {
public:
    TopNavView(std::initializer_list<ElementView> leading = {}) {
        init(skiff::Text("").fg(0xFFFFFF), leading);
    }

    TopNavView(const std::string& title,
               std::initializer_list<ElementView> leading = {}) {
        init(skiff::Text(title).fg(0xFFFFFF), leading);
    }

    TopNavView(ElementView titleElement,
               std::initializer_list<ElementView> leading = {}) {
        init(titleElement, leading);
    }

    // 设置标题;可传字符串(默认白色),也可传已设置好样式的 Text ElementView
    TopNavView& title(const std::string& t) {
        title_ = skiff::Text(t).fg(0xFFFFFF);
        return *this;
    }

    TopNavView& title(ElementView t) {
        title_ = t;
        return *this;
    }

    // 重新设置左侧操作按钮
    TopNavView& leading(std::initializer_list<ElementView> leading) {
        leading_ = leading;
        return *this;
    }

    Element build() const override {
        Element row;
        row.kind = Element::Row;
        row.options.height = 48;
        row.options.center = true;
        row.options.paddingLeft = row.options.paddingRight = 8;
        row.options.spacingPx = 12;
        for (size_t i = 0; i < leading_.size(); ++i) {
            row.children.push_back(leading_[i].build());
        }
        row.children.push_back(title_.build());
        row.children.push_back(skiff::Spacer().build());
        return row;
    }

    // ---- 左侧常用按钮工厂(文案由调用方传入,通常 tr(...)) ----
    static ElementView routerHome(Router& router, const std::string& label) {
        return skiff::Button(label, [&router] { router.home(); })
            .size(140, 36).bg(0x26303B).fg(0xFFFFFF);
    }

    // 返回上一页按钮(如果上一页状态为 Dead,会自动跳过)
    static ElementView routerPrev(Router& router, const std::string& label) {
        return skiff::Button(label, [&router] { router.pop(); })
            .size(100, 36).bg(0x26303B).fg(0xFFFFFF);
    }

private:
    std::vector<ElementView> leading_;
    ElementView title_;

    void init(ElementView title, std::initializer_list<ElementView> leading) {
        title_ = title;
        leading_ = leading;
    }
};

// 兼容旧写法的工厂函数
inline TopNavView TopNav(std::initializer_list<ElementView> leading = {}) {
    return TopNavView(leading);
}

inline TopNavView TopNav(const std::string& title,
                         std::initializer_list<ElementView> leading = {}) {
    return TopNavView(title, leading);
}

inline TopNavView TopNav(ElementView titleElement,
                         std::initializer_list<ElementView> leading = {}) {
    return TopNavView(titleElement, leading);
}

} // namespace components
} // namespace skiff
