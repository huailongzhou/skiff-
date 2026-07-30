// skiff —— 声明式 UI 核心(后端无关)
//
// Element 是对界面的一棵纯数据描述:有什么控件、什么文本、什么层级、什么样式。
// 它不持有任何后端对象,由 Backend 把它"挂载"成真实的原生控件树。
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace skiff {

struct Element {
    enum Kind {
        Column,  // 纵向容器(对应 SwiftUI 的 VStack)
        Row,     // 横向容器(对应 SwiftUI 的 HStack)
        Text,    // 文本
        Button,  // 按钮(text 为文字;children 非空时用 children 做按钮内容)
        Spacer   // 弹性空白(对应 SwiftUI 的 Spacer)
    };

    Kind kind;
    std::string text;               // Text / Button 的文字
    int spacing;                    // Column / Row 的子项间距(px)
    int paddingPx;                  // 内边距(px)
    int width, height;              // px;0 表示按内容自适应
    uint32_t bgColor;               // 背景色(RGB hex),hasBg 为 true 时生效
    bool hasBg;
    uint32_t fgColor;               // 文字颜色,hasFg 为 true 时生效
    bool hasFg;
    int fontPx;                     // 字号(px),0 = 后端默认
    std::string ttfPath;            // TTF 字体文件路径(空 = 用后端内置字体)
    bool center;                    // 容器:子项主轴/交叉轴居中
    std::vector<Element> children;  // 容器的子节点 / Button 的自定义内容
    std::function<void()> onTap;    // Button 的点击回调

    Element()
        : kind(Text), spacing(0), paddingPx(0), width(0), height(0),
          bgColor(0), hasBg(false), fgColor(0), hasFg(false),
          fontPx(0), center(false) {}

    // ---- 链式修饰器(类似 SwiftUI 的 modifier) ----
    Element size(int w, int h) const { Element e = *this; e.width = w; e.height = h; return e; }
    Element bg(uint32_t rgb) const   { Element e = *this; e.bgColor = rgb; e.hasBg = true; return e; }
    Element fg(uint32_t rgb) const   { Element e = *this; e.fgColor = rgb; e.hasFg = true; return e; }
    Element font(int px) const       { Element e = *this; e.fontPx = px; return e; }
    Element pad(int px) const        { Element e = *this; e.paddingPx = px; return e; }
    Element ttf(const char* path, int px) const { Element e = *this; e.ttfPath = path; e.fontPx = px; return e; }
    Element centered() const         { Element e = *this; e.center = true; return e; }
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
    return e;
}

// 自定义内容的按钮:Button({ ... 内容 ... }, action)
inline Element Button(std::vector<Element> content, std::function<void()> onTap) {
    Element e;
    e.kind = Element::Button;
    e.children = std::move(content);
    e.onTap = std::move(onTap);
    return e;
}

inline Element Spacer() {
    Element e;
    e.kind = Element::Spacer;
    return e;
}

inline Element VStack(std::vector<Element> children, int spacing = 0) {
    Element e;
    e.kind = Element::Column;
    e.spacing = spacing;
    e.children = std::move(children);
    return e;
}

inline Element HStack(std::vector<Element> children, int spacing = 0) {
    Element e;
    e.kind = Element::Row;
    e.spacing = spacing;
    e.children = std::move(children);
    return e;
}

} // namespace skiff
