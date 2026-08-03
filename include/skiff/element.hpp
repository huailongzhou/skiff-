// skiff —— 声明式 UI 核心(后端无关)
//
// Element 是对界面的一棵纯数据描述:有什么控件、什么层级、什么样式。
// 它不持有任何后端对象,由 Backend 把它"挂载"成真实的原生控件树。
//
// 常用样式与布局属性统一放在 elements::attrOptions 中管理,
// Element 通过 `options` 成员访问尺寸、颜色、字体、内边距、动画等。
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "elements/attr_options.hpp"

namespace skiff {

struct Element {
    enum Kind {
        Column,   // 纵向容器(对应 SwiftUI 的 VStack)
        Row,      // 横向容器(对应 SwiftUI 的 HStack)
        Text,     // 文本
        Button,   // 按钮(text 为文字;children 非空时用 children 做按钮内容)
        Spacer,   // 弹性空白(对应 SwiftUI 的 Spacer)
        Slider,   // 滑条
        TabView,  // 页签容器(对应 SwiftUI 的 TabView;children 为各页内容,
                  // 每个 child 的 text 作为该页标题,见 Tab())
        External, // 外部回调声明:不渲染 UI,由后端/板子代码按名字执行具体功能
        TapArea   // 透明点击区:不渲染 UI,用于捕获点击事件(如关闭浮层)
    };

    Kind kind;
    std::string text;               // Text / Button 的文字
    elements::attrOptions options;  // 默认状态(STATE_DEFAULT)下的常用样式
    // 各状态下的样式覆盖,参考 LVGL 的 state selector
    std::map<elements::state, elements::attrOptions> stateStyles;
    int min, max, value;            // Slider 用
    std::function<void(int)> onValueChange; // Slider 值变化回调
    std::vector<Element> children;       // 容器的子节点 / Button 的自定义内容
    std::function<void()> onTap;         // Button 的点击回调

    // External 元素:声明式外部调用
    std::string externalName;            // 外部功能名
    std::vector<std::string> externalArgs; // 调用参数(字符串化后交给板子代码解析)

    // Button 外部点击:不感知具体实现,由后端调用 registerExternal 注册的处理器
    std::string externalTapName;
    std::vector<std::string> externalTapArgs;

    Element()
        : kind(Text), options(), stateStyles(),
          min(0), max(100), value(0) {}

    // ---- 链式修饰器(类似 SwiftUI 的 modifier) ----
    // 所有样式最终写入 options
    Element size(int w, int h) const {
        Element e = *this;
        e.options.width = w; e.options.height = h;
        return e;
    }

    Element sizePct(int w, int h) const {
        Element e = *this;
        e.options.widthPct = w; e.options.heightPct = h;
        return e;
    }

    Element bg(uint32_t rgb) const {
        Element e = *this;
        e.options.bgColor = rgb; e.options.hasBg = true;
        return e;
    }

    Element fg(uint32_t rgb) const {
        Element e = *this;
        e.options.fgColor = rgb; e.options.hasFg = true;
        return e;
    }

    Element font(int px) const {
        Element e = *this; e.options.fontPx = px; return e;
    }

    Element itemHeight(int px) const {
        Element e = *this; e.options.itemHeightPx = px; return e;
    }

    Element pad(int px) const {
        Element e = *this;
        e.options.paddingPx = px;
        e.options.paddingTop = e.options.paddingBottom =
            e.options.paddingLeft = e.options.paddingRight = px;
        return e;
    }

    Element padTop(int px) const {
        Element e = *this; e.options.paddingTop = px; return e;
    }

    Element padBottom(int px) const {
        Element e = *this; e.options.paddingBottom = px; return e;
    }

    Element padLeft(int px) const {
        Element e = *this; e.options.paddingLeft = px; return e;
    }

    Element padRight(int px) const {
        Element e = *this; e.options.paddingRight = px; return e;
    }

    Element ttf(const char* path, int px) const {
        Element e = *this;
        e.options.ttfPath = path; e.options.fontPx = px;
        return e;
    }

    Element centered() const {
        Element e = *this; e.options.center = true; return e;
    }

    Element expand() const {
        Element e = *this; e.options.flexGrow = true; return e;
    }

    Element floating() const {
        Element e = *this; e.options.isFloating = true; return e;
    }

    Element radius(int px) const {
        Element e = *this; e.options.radiusPx = px; e.options.hasRadiusPx = true; return e;
    }

    Element spacing(int px) const {
        Element e = *this; e.options.spacingPx = px; return e;
    }

    Element slideInRight() const {
        Element e = *this; e.options.animation = SlideInRight; return e;
    }

    Element slideInDown() const {
        Element e = *this; e.options.animation = SlideInDown; return e;
    }

    Element key(const char* k) const {
        Element e = *this; e.options.keyId = k; return e;
    }

    Element scrollHorizontal() const {
        Element e = *this; e.options.scrollDir = ScrollHorizontal; return e;
    }

    Element scrollVertical() const {
        Element e = *this; e.options.scrollDir = ScrollVertical; return e;
    }

    Element scrollSnapStart() const {
        Element e = *this; e.options.scrollSnap = SnapStart; return e;
    }

    Element scrollSnapCenter() const {
        Element e = *this; e.options.scrollSnap = SnapCenter; return e;
    }

    Element alignLeft() const {
        Element e = *this; e.options.hAlign = elements::HAlignStart; return e;
    }

    Element alignCenter() const {
        Element e = *this; e.options.hAlign = elements::HAlignCenter; return e;
    }

    Element alignRight() const {
        Element e = *this; e.options.hAlign = elements::HAlignEnd; return e;
    }

    // 声明 Button 点击时触发 external 回调(由后端 registerExternal 实现)
    Element externalTap(const std::string& name,
                        const std::vector<std::string>& args = {}) const {
        Element e = *this;
        e.externalTapName = name;
        e.externalTapArgs = args;
        return e;
    }

    // 批量应用 attrOptions 到指定状态(参考 LVGL 的 state selector)
    // 默认状态(state())会同时写入 options,保持后端兼容
    Element& applyOptions(const elements::state& s,
                          const elements::attrOptions& opt) {
        if (s.value == elements::state::Default) {
            options = opt;
        }
        stateStyles[s] = opt;
        return *this;
    }

    // 旧签名的兼容版本,等价于 applyOptions(state(), opt)
    Element& applyOptions(const elements::attrOptions& opt) {
        return applyOptions(elements::state(), opt);
    }
};

// ---- 构建函数:写法上对齐 SwiftUI ----

inline Element Text(std::string text) {
    Element e;
    e.kind = Element::Text;
    e.text = std::move(text);
    return e;
}

inline Element Button(std::string label, std::function<void()> onTap) {
    Element e;
    e.kind = Element::Button;
    e.text = std::move(label);
    e.onTap = std::move(onTap);
    e.options.center = true;  // 按钮内容默认居中
    return e;
}

// 自定义内容的按钮:Button({ ... 内容 ... }, action)
inline Element Button(std::vector<Element> content, std::function<void()> onTap) {
    Element e;
    e.kind = Element::Button;
    e.children = std::move(content);
    e.onTap = std::move(onTap);
    e.options.center = true;  // 按钮内容默认居中
    return e;
}

inline Element Spacer() {
    Element e;
    e.kind = Element::Spacer;
    return e;
}

inline Element Slider(int value, int min, int max,
                      std::function<void(int)> onValueChange) {
    Element e;
    e.kind = Element::Slider;
    e.value = value;
    e.min = min;
    e.max = max;
    e.onValueChange = std::move(onValueChange);
    return e;
}

inline Element VStack(std::vector<Element> children, int spacing = 0) {
    Element e;
    e.kind = Element::Column;
    e.options.spacingPx = spacing;
    e.children = std::move(children);
    return e;
}

inline Element HStack(std::vector<Element> children, int spacing = 0) {
    Element e;
    e.kind = Element::Row;
    e.options.spacingPx = spacing;
    e.children = std::move(children);
    return e;
}

// 一个页签:title 为标题,content 为该页内容(复用 content 的 text 字段存标题)
inline Element Tab(std::string title, Element content) {
    content.text = std::move(title);
    return content;
}

// 页签容器:TabView({ Tab("标题1", 内容1), Tab("标题2", 内容2) })
// font()/ttf()/fg() 作用于页签栏文字
inline Element TabView(std::vector<Element> tabs) {
    Element e;
    e.kind = Element::TabView;
    e.children = std::move(tabs);
    return e;
}

// 外部回调声明:不渲染 UI,由后端/板子代码按名字实现。
// 典型用法:放在页面 body 中或作为 Slider/Button 回调的替代,把具体硬件操作
// 交给 registerExternal 注册的处理器。
//   External("setBrightness", {std::to_string(brightness.get())})
inline Element External(std::string name,
                        std::vector<std::string> args = {}) {
    Element e;
    e.kind = Element::External;
    e.externalName = std::move(name);
    e.externalArgs = std::move(args);
    return e;
}

// 透明点击区:不渲染任何 UI,只响应点击。
// 典型用法:弹窗/下拉菜单背后的全屏遮罩,点外部关闭。
inline Element TapArea(std::function<void()> onTap) {
    Element e;
    e.kind = Element::TapArea;
    e.onTap = std::move(onTap);
    return e;
}

} // namespace skiff
