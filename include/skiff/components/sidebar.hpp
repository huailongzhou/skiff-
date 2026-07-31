// Sidebar:左侧一级菜单 + 右侧内容区的组合组件(纯 DSL 组合,后端无关)。
// 选中态由调用方持有的 State<int> 驱动,切换时内容区默认带右滑入动画。
#pragma once

#include <string>
#include <vector>

#include "../element.hpp"
#include "../state.hpp"

namespace skiff {
namespace components {

struct SidebarItem {
    std::string title;    // 菜单项文字
    Element content;      // 该菜单项对应的内容区
};

struct SidebarOptions {
    int width, height;          // 整体尺寸(px),0 = 按内容自适应
    int menuWidth;              // 侧栏宽度
    int menuPad;                // 侧栏内边距
    int itemWidth, itemHeight;  // 菜单项尺寸
    int itemSpacing;            // 菜单项间距
    uint32_t activeBg;          // 选中项背景色
    uint32_t inactiveBg;        // 未选中项背景色
    uint32_t itemFg;            // 菜单项文字颜色
    uint32_t contentBg;         // 内容区背景色(hasContentBg 为 true 时生效)
    bool hasContentBg;
    std::string ttfPath;        // 菜单 TTF 字体路径(空 = 后端内置字体)
    int fontPx;                 // 菜单字号
    bool slideIn;               // 切换时内容区右滑入动画
    bool hasMenuFooter;
    Element menuFooter;         // 侧栏底部内容(如返回按钮),hasMenuFooter 为 true 时生效

    SidebarOptions()
        : width(0), height(0), menuWidth(200), menuPad(10),
          itemWidth(180), itemHeight(48), itemSpacing(8),
          activeBg(0x1565D8), inactiveBg(0x26303B), itemFg(0xFFFFFF),
          contentBg(0), hasContentBg(false), fontPx(18),
          slideIn(true), hasMenuFooter(false) {}
};

namespace detail {

inline Element sidebarMenuItem(const SidebarItem& item, int idx, int active,
                               State<int>& tab, const SidebarOptions& opt) {
    Element e = Button(item.title, [&tab, idx] { tab.set(idx); })
        .size(opt.itemWidth, opt.itemHeight)
        .bg(idx == active ? opt.activeBg : opt.inactiveBg)
        .fg(opt.itemFg);
    if (!opt.ttfPath.empty()) e = e.ttf(opt.ttfPath.c_str(), opt.fontPx);
    else if (opt.fontPx > 0)  e = e.font(opt.fontPx);
    return e;
}

} // namespace detail

// items 至少一项;active 越界时回退到 0
inline Element Sidebar(const std::vector<SidebarItem>& items, int active,
                       State<int>& tab,
                       const SidebarOptions& opt = SidebarOptions()) {
    // 侧栏:菜单项 + 弹性空白 + 可选底部内容
    std::vector<Element> menu;
    for (size_t i = 0; i < items.size(); ++i) {
        menu.push_back(detail::sidebarMenuItem(items[i], (int)i, active, tab, opt));
    }
    menu.push_back(Spacer());
    if (opt.hasMenuFooter) menu.push_back(opt.menuFooter);
    Element sidebar = VStack(menu, opt.itemSpacing)
        .size(opt.menuWidth, opt.height)
        .pad(opt.menuPad);

    // 内容区:当前项;key 用索引区分,切换时整棵重建以播放入场动画
    int cur = active;
    if (cur < 0 || cur >= (int)items.size()) cur = 0;
    Element content = items.empty() ? VStack({}) : items[cur].content;
    content = content.key(std::to_string(cur).c_str());
    if (opt.slideIn) content = content.slideInRight();
    Element contentWrap = VStack({content}, 0).size(0, opt.height).expand();
    if (opt.hasContentBg) contentWrap = contentWrap.bg(opt.contentBg);

    return HStack({sidebar, contentWrap}, 0).size(opt.width, opt.height);
}

} // namespace components
} // namespace skiff
