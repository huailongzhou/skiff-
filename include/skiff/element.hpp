// skiff —— 声明式 UI 核心(后端无关)
//
// Element 是对界面的一棵纯数据描述:有什么控件、什么层级、什么样式。
// 它不持有任何后端对象,由 Backend 把它"挂载"成真实的原生控件树。
//
// ElementView 是前端的 DSL 包装,提供链式 modifier 与延迟 build() 能力。
// 复杂组件(如 List)可以继承 ElementView 并重写 build() 来展开成通用 Element 树。
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "elements/attr_options.hpp"
#include "elements/list_options.hpp"

namespace skiff {

// 前向声明
class ElementView;

// 小众数据:仅 Slider / 带状态样式的节点才分配,Element 本体保持精简。
struct RareData {
    int min, max, value;                        // Slider 用
    std::function<void(int)> onValueChange;     // Slider 值变化回调
    // 各状态下的样式覆盖,参考 LVGL 的 state selector
    std::map<elements::state, elements::attrOptions> stateStyles;
    RareData() : min(0), max(100), value(0) {}
};

struct Element {
    enum Kind {
        Column,   // 纵向容器(对应 SwiftUI 的 VStack)
        Row,      // 横向容器(对应 SwiftUI 的 HStack)
        Text,     // 文本
        Button,   // 按钮(text 为文字;children 非空时用 children 做按钮内容)
        Spacer,   // 弹性空白(对应 SwiftUI 的 Spacer)
        Slider,   // 滑条
        TabView,  // 页签容器(对应 SwiftUI 的 TabView;children 为各页内容,
                  // 每个 child 的 tabTitle 作为该页标题,见 Tab())
        TapArea,  // 透明点击区:不渲染 UI,用于捕获点击事件(如关闭浮层)
    };

    Kind kind;
    std::string text;               // Text / Button 的文字
    std::string tabTitle;           // Tab() 的页签标题(与 content.text 分离,不覆盖内容文字)
    elements::attrOptions options;  // 默认状态(STATE_DEFAULT)下的常用样式
    std::vector<Element> children;       // 容器的子节点 / Button 的自定义内容
    std::function<void()> onTap;         // Button 的点击回调
    std::unique_ptr<RareData> rare;      // Slider / 状态样式节点才分配

    Element() : kind(Text), options() {}

    // unique_ptr 不可拷贝:深拷贝 rare(没有 rare 的节点零开销)
    Element(const Element& o)
        : kind(o.kind), text(o.text), tabTitle(o.tabTitle), options(o.options),
          children(o.children), onTap(o.onTap),
          rare(o.rare ? new RareData(*o.rare) : nullptr) {}
    Element& operator=(const Element& o) {
        if (this != &o) {
            kind = o.kind;
            text = o.text;
            tabTitle = o.tabTitle;
            options = o.options;
            children = o.children;
            onTap = o.onTap;
            rare.reset(o.rare ? new RareData(*o.rare) : nullptr);
        }
        return *this;
    }
    Element(Element&&) = default;
    Element& operator=(Element&&) = default;

    // 仅 Slider / 状态样式节点使用:没有 rare 时分配
    RareData& ensureRare() {
        if (!rare) rare.reset(new RareData());
        return *rare;
    }

    // 批量应用 attrOptions 到指定状态(参考 LVGL 的 state selector)
    // 默认状态(state())会同时写入 options,保持后端兼容
    Element& applyOptions(const elements::state& s,
                          const elements::attrOptions& opt) {
        if (s.value == elements::state::Default) {
            options = opt;
        }
        ensureRare().stateStyles[s] = opt;
        return *this;
    }

    // 旧签名的兼容版本,等价于 applyOptions(state(), opt)
    Element& applyOptions(const elements::attrOptions& opt) {
        return applyOptions(elements::state(), opt);
    }

    // Element 是纯数据结构,所有链式 modifier 统一在 ElementView 上实现。
};

// 前端 DSL 包装:持有 Element 并提供链式 modifier。
// 复杂组件可继承此类并重写 build() 来延迟展开。
class ElementView {
public:
    ElementView() : e_(new Element()) {}
    ElementView(const Element& e) : e_(new Element(e)) {}
    ElementView(Element&& e) : e_(new Element(std::move(e))) {}

    virtual ~ElementView() {}

    // 返回展开后的 Element 树。默认直接返回内部 Element;
    // 子类(如 ListView)可重写此方法来生成复杂结构。
    virtual Element build() const { return *e_; }

    // 隐式转换为 Element,方便直接作为 Element 返回
    operator Element() const { return build(); }

    // ---- 链式修饰器(类似 SwiftUI 的 modifier) ----
    // 所有样式最终写入内部 Element 的 options
    ElementView& size(int w, int h) {
        e_->options.width = w; e_->options.height = h;
        return *this;
    }

    ElementView& sizePct(int w, int h) {
        e_->options.widthPct = w; e_->options.heightPct = h;
        return *this;
    }

    ElementView& widthPct(int w) {
        e_->options.widthPct = w;
        return *this;
    }

    ElementView& heightPct(int h) {
        e_->options.heightPct = h;
        return *this;
    }

    ElementView& bg(uint32_t rgb) {
        e_->options.bgColor = rgb; e_->options.hasBg = true;
        return *this;
    }

    ElementView& fg(uint32_t rgb) {
        e_->options.fgColor = rgb; e_->options.hasFg = true;
        return *this;
    }

    ElementView& borderBottom(uint32_t rgb, int widthPx = 1) {
        e_->options.borderBottomColor = rgb;
        e_->options.hasBorderBottom = true;
        e_->options.borderBottomWidth = widthPx;
        return *this;
    }

    ElementView& font(int px) {
        e_->options.fontPx = px; return *this;
    }

    ElementView& itemHeight(int px) {
        e_->options.itemHeightPx = px; return *this;
    }

    ElementView& pad(int px) {
        e_->options.paddingPx = px;
        e_->options.paddingTop = e_->options.paddingBottom =
            e_->options.paddingLeft = e_->options.paddingRight = px;
        return *this;
    }

    ElementView& padTop(int px)    { e_->options.paddingTop = px; return *this; }
    ElementView& padBottom(int px) { e_->options.paddingBottom = px; return *this; }
    ElementView& padLeft(int px)   { e_->options.paddingLeft = px; return *this; }
    ElementView& padRight(int px)  { e_->options.paddingRight = px; return *this; }

    ElementView& ttf(const char* path, int px) {
        e_->options.ttfPath = path; e_->options.fontPx = px;
        return *this;
    }

    ElementView& centered() { e_->options.center = true; return *this; }
    ElementView& expand()   { e_->options.flexGrow = true; return *this; }
    ElementView& floating() { e_->options.isFloating = true; return *this; }

    ElementView& radius(int px) {
        e_->options.radiusPx = px; e_->options.hasRadiusPx = true; return *this;
    }

    ElementView& spacing(int px) {
        e_->options.spacingPx = px; return *this;
    }

    ElementView& slideInRight() {
        e_->options.animation = SlideInRight; return *this;
    }

    ElementView& slideInDown() {
        e_->options.animation = SlideInDown; return *this;
    }

    ElementView& key(const char* k) {
        e_->options.keyId = k; return *this;
    }

    ElementView& scrollHorizontal() {
        e_->options.scrollDir = ScrollHorizontal; return *this;
    }

    ElementView& scrollVertical() {
        e_->options.scrollDir = ScrollVertical; return *this;
    }

    ElementView& scrollSnapStart()  { e_->options.scrollSnap = SnapStart;  return *this; }
    ElementView& scrollSnapCenter() { e_->options.scrollSnap = SnapCenter; return *this; }
    ElementView& scrollSnapEnd()    { e_->options.scrollSnap = SnapEnd;    return *this; }

    ElementView& alignLeft()   { e_->options.hAlign = elements::HAlignStart;  return *this; }
    ElementView& alignCenter() { e_->options.hAlign = elements::HAlignCenter; return *this; }
    ElementView& alignRight()  { e_->options.hAlign = elements::HAlignEnd;    return *this; }

    // 显式在 ElementView 与子类之间转换,用于链式调用中切换返回类型
    template <typename T>
    T& as() {
        return static_cast<T&>(*this);
    }

    ElementView& applyOptions(const elements::state& s,
                              const elements::attrOptions& opt) {
        if (s.value == elements::state::Default) {
            e_->options = opt;
        }
        e_->ensureRare().stateStyles[s] = opt;
        return *this;
    }

    ElementView& applyOptions(const elements::attrOptions& opt) {
        return applyOptions(elements::state(), opt);
    }

protected:
    std::shared_ptr<Element> e_;
};

// ---- 构建函数:写法上对齐 SwiftUI ----

inline ElementView Text(std::string text) {
    Element e;
    e.kind = Element::Text;
    e.text = std::move(text);
    return ElementView(std::move(e));
}

inline ElementView Button(std::string label, std::function<void()> onTap) {
    Element e;
    e.kind = Element::Button;
    e.text = std::move(label);
    e.onTap = std::move(onTap);
    e.options.center = true;  // 按钮内容默认居中
    return ElementView(std::move(e));
}

// 自定义内容的按钮:Button({ ... 内容 ... }, action)
inline ElementView Button(std::vector<Element> content, std::function<void()> onTap) {
    Element e;
    e.kind = Element::Button;
    e.options.center = true;  // 按钮内容默认居中
    e.onTap = std::move(onTap);
    e.children = std::move(content);
    return ElementView(std::move(e));
}

inline ElementView Spacer() {
    Element e;
    e.kind = Element::Spacer;
    return ElementView(std::move(e));
}

inline ElementView Slider(int value, int min, int max,
                          std::function<void(int)> onValueChange) {
    Element e;
    e.kind = Element::Slider;
    RareData& rd = e.ensureRare();
    rd.value = value;
    rd.min = min;
    rd.max = max;
    rd.onValueChange = std::move(onValueChange);
    return ElementView(std::move(e));
}

inline ElementView VStack(std::vector<Element> children, int spacing = 0) {
    Element e;
    e.kind = Element::Column;
    e.options.spacingPx = spacing;
    e.children = std::move(children);
    return ElementView(std::move(e));
}

inline ElementView HStack(std::vector<Element> children, int spacing = 0) {
    Element e;
    e.kind = Element::Row;
    e.options.spacingPx = spacing;
    e.children = std::move(children);
    return ElementView(std::move(e));
}

// 一个页签:title 为标题,content 为该页内容。
// 标题存在独立的 tabTitle 字段,不会覆盖 content 自身的 text(如 Text 内容)。
inline ElementView Tab(std::string title, Element content) {
    content.tabTitle = std::move(title);
    return ElementView(std::move(content));
}

// 页签容器:TabView({ Tab("标题1", 内容1), Tab("标题2", 内容2) })
// font()/ttf()/fg() 作用于页签栏文字
inline ElementView TabView(std::vector<Element> tabs) {
    Element e;
    e.kind = Element::TabView;
    e.children = std::move(tabs);
    return ElementView(std::move(e));
}

// 透明点击区:不渲染任何 UI,只响应点击。
inline ElementView TapArea(std::function<void()> onTap) {
    Element e;
    e.kind = Element::TapArea;
    e.onTap = std::move(onTap);
    return ElementView(std::move(e));
}

} // namespace skiff
