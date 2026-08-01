// TopNav:页面顶部导航条(纯 DSL 组合,后端无关)。
// 左侧可放置返回等操作元素,标题紧随其后。
#pragma once

#include <string>

#include "../element.hpp"

namespace skiff {
namespace components {

struct TopNavOptions {
    int width, height;          // 整体尺寸(px),0 = 按内容自适应
    int pad;                    // 左右内边距
    int spacing;                // leading 与标题的间距
    uint32_t bgColor;           // 背景色(hasBg 为 true 时生效)
    bool hasBg;
    uint32_t titleFg;           // 标题文字颜色
    std::string ttfPath;        // 标题 TTF 字体路径(空 = 后端内置字体)
    int fontPx;                 // 标题字号
    bool hasLeading;
    Element leading;            // 左侧元素(如返回按钮),hasLeading 为 true 时生效

    TopNavOptions()
        : width(0), height(48), pad(8), spacing(12),
          bgColor(0), hasBg(false), titleFg(0xFFFFFF),
          fontPx(20), hasLeading(false) {}
};

inline Element TopNav(const std::string& title,
                      const TopNavOptions& opt = TopNavOptions()) {
    std::vector<Element> children;
    if (opt.hasLeading) children.push_back(opt.leading);
    Element t = Text(title).fg(opt.titleFg);
    if (!opt.ttfPath.empty()) t = t.ttf(opt.ttfPath.c_str(), opt.fontPx);
    else if (opt.fontPx > 0)  t = t.font(opt.fontPx);
    children.push_back(t);
    children.push_back(Spacer());  // 吃掉剩余空间,让内容靠左

    Element bar = HStack(children, opt.spacing)
        .size(opt.width, opt.height)
        .padLeft(opt.pad).padRight(opt.pad)
        .centered();
    if (opt.hasBg) bar = bar.bg(opt.bgColor);
    return bar;
}

} // namespace components
} // namespace skiff
