// TopNav:页面顶部导航条(纯 DSL 组合,后端无关)。
// TopNav 本身就是一个 Element,所有 Element 的链式 modifier(size/bg/ttf 等)都可用。
// 左侧可放置返回等操作元素,标题紧随其后。
#pragma once

#include <initializer_list>
#include <string>
#include <vector>

#include "../element.hpp"
#include "router.hpp"

namespace skiff {
namespace components {

class TopNav : public Element {
public:
    TopNav(std::initializer_list<Element> leading = {}) {
        init(skiff::Text("").fg(0xFFFFFF), leading);
    }

    TopNav(const std::string& title,
           std::initializer_list<Element> leading = {}) {
        init(skiff::Text(title).fg(0xFFFFFF), leading);
    }

    TopNav(Element titleElement,
           std::initializer_list<Element> leading = {}) {
        init(titleElement, leading);
    }

    // 设置标题;可传字符串(默认白色),也可传已设置好样式的 Text Element
    TopNav& title(const std::string& t) {
        title_ = skiff::Text(t).fg(0xFFFFFF);
        rebuildChildren();
        return *this;
    }

    TopNav& title(Element t) {
        title_ = t;
        rebuildChildren();
        return *this;
    }

    // 重新设置左侧操作按钮
    TopNav& leading(std::initializer_list<Element> leading) {
        leading_ = leading;
        rebuildChildren();
        return *this;
    }

    // ---- 左侧常用按钮工厂 ----
    // 返回主页按钮;可通过 Element 的链式 modifier(.ttf/.size/.bg 等)继续调整样式
    static Element routerHome(Router& router) {
        return skiff::Button("返回主页", [&router] { router.home(); })
            .size(140, 36).bg(0x26303B).fg(0xFFFFFF);
    }

    // 返回上一页按钮(如果上一页状态为 Dead,会自动跳过)
    static Element routerPrev(Router& router) {
        return skiff::Button("返回", [&router] { router.pop(); })
            .size(100, 36).bg(0x26303B).fg(0xFFFFFF);
    }

private:
    std::vector<Element> leading_;
    Element title_;

    void init(Element title, std::initializer_list<Element> leading) {
        kind = Row;
        options.height = 48;
        options.center = true;
        options.paddingLeft = options.paddingRight = 8;
        options.spacingPx = 12;
        title_ = title;
        leading_ = leading;
        rebuildChildren();
    }

    void rebuildChildren() {
        children = leading_;
        children.push_back(title_);
        children.push_back(skiff::Spacer());
    }
};

} // namespace components
} // namespace skiff
