// TabView:左侧页签菜单 + 右侧内容区的组合组件(纯 DSL 组合,后端无关)。
// 现在 TabView 本身就是一个 Element,所有 Element 的链式 modifier 都可用。
// 选中态由调用方持有的 State<int> 驱动,切换时内容区默认带右滑入动画。
//
// 所有与背景色相关的样式统一通过 applyBgOption({{part, state, color}, ...}) 设置,
// 不再提供 activeBg/inactiveBg/contentBg 等单独方法。
//
// 注意:与 element 层的 skiff::TabView(LVGL 页签容器)同名不同义,
// 使用时带命名空间:skiff::components::TabView。
#pragma once

#include <map>
#include <string>
#include <vector>

#include "../element.hpp"
#include "../state.hpp"

namespace skiff {
namespace components {

struct TabViewItem {
    std::string title;    // 页签文字
    Element content;      // 该页签对应的内容区
};

// TabView 专用属性描述类
struct TabViewOptions {
    int menuWidth_, menuPad_;
    int itemWidth_, itemHeight_, itemSpacing_;
    uint32_t itemFg_;
    int fontPx_;
    bool slideIn_;
    std::string ttfPath_;

    TabViewOptions()
        : menuWidth_(200), menuPad_(10),
          itemWidth_(180), itemHeight_(48), itemSpacing_(8),
          itemFg_(0xFFFFFF), fontPx_(18), slideIn_(true) {}

    TabViewOptions& menuWidth(int w) { menuWidth_ = w; return *this; }
    TabViewOptions& menuPad(int p) { menuPad_ = p; return *this; }
    TabViewOptions& itemSize(int w, int h) {
        itemWidth_ = w; itemHeight_ = h; return *this;
    }
    TabViewOptions& itemSpacing(int s) { itemSpacing_ = s; return *this; }
    TabViewOptions& itemFg(uint32_t c) { itemFg_ = c; return *this; }
    TabViewOptions& font(int px) { ttfPath_.clear(); fontPx_ = px; return *this; }
    TabViewOptions& ttf(const char* path, int px) {
        ttfPath_ = path; fontPx_ = px; return *this;
    }
    TabViewOptions& slideIn(bool s = true) { slideIn_ = s; return *this; }
};

namespace tabview {

// TabView 的组成部分标记
struct part {
    enum type { first, content } t;
};

// 左侧一级菜单(页签栏)
inline part first() {
    part p; p.t = part::first; return p;
}

// 右侧内容区
inline part content() {
    part p; p.t = part::content; return p;
}

// 背景色选项: part + state + color
struct bgOption {
    part p;
    elements::state st;
    uint32_t color;

    bgOption(part pp, const elements::state& ss, uint32_t c)
        : p(pp), st(ss), color(c) {}
};

} // namespace tabview

// TabView 类:继承 Element,可直接放入 VStack/HStack
class TabView : public Element {
public:
    TabView(const std::vector<TabViewItem>& items, State<int>& tab)
        : items_(items), tab_(&tab) {
        kind = Row;
        options.spacingPx = 0;
        rebuild();
    }

    // 对指定部分/状态批量应用样式(参考 LVGL 的 state selector)
    TabView& applyOptions(tabview::part p,
                          const elements::state& s,
                          const elements::attrOptions& opt) {
        if (p.t == tabview::part::first) {
            firstStyles_[s] = opt;
        } else {
            contentStyles_[s] = opt;
        }
        rebuild();
        return *this;
    }

    // 批量设置背景色:{{part, state, color}, ...}
    // 对 first 部分支持 state::selected()/unselected() 表示选中/未选中项,
    // 也支持 elements::state::pressed() 等交互状态。
    TabView& applyBgOption(std::initializer_list<tabview::bgOption> opts) {
        bgOptions_.assign(opts);
        rebuild();
        return *this;
    }

    // 设置一级菜单项尺寸
    TabView& itemSize(int w, int h) {
        tabOpts_.itemSize(w, h); rebuild(); return *this;
    }

    // 设置菜单栏宽度
    TabView& menuWidth(int w) {
        tabOpts_.menuWidth_ = w; rebuild(); return *this;
    }

    // 页签项文字颜色
    TabView& itemFg(uint32_t c) {
        tabOpts_.itemFg_ = c; rebuild(); return *this;
    }

    // 页签字体
    TabView& ttf(const char* path, int px) {
        tabOpts_.ttf(path, px); rebuild(); return *this;
    }

    TabView& font(int px) {
        tabOpts_.font(px); rebuild(); return *this;
    }

private:
    std::vector<TabViewItem> items_;
    State<int>* tab_;
    TabViewOptions tabOpts_;

    std::vector<tabview::bgOption> bgOptions_;
    std::map<elements::state, elements::attrOptions> firstStyles_;
    std::map<elements::state, elements::attrOptions> contentStyles_;

    // 判断某个 bgOption 是否为选中/未选中伪状态
    static bool isSelected(const elements::state& s) {
        return s.value == elements::state::Selected;
    }
    static bool isUnselected(const elements::state& s) {
        return s.value == elements::state::Unselected;
    }

    void rebuild() {
        if (items_.empty()) return;  // 防御:空页签列表时无内容可构建
        int active = tab_->get();
        if (active < 0 || active >= (int)items_.size()) active = 0;
        const int height = options.height > 0 ? options.height : 432;

        // 左侧页签栏
        std::vector<Element> menu;
        for (int i = 0; i < (int)items_.size(); ++i) {
            State<int>* tabPtr = tab_;
            int idx = i;
            Element btn = skiff::Button(items_[i].title,
                                        [tabPtr, idx] { tabPtr->set(idx); })
                .size(tabOpts_.itemWidth_, tabOpts_.itemHeight_)
                .fg(tabOpts_.itemFg_);
            if (!tabOpts_.ttfPath_.empty()) btn = btn.ttf(tabOpts_.ttfPath_.c_str(), tabOpts_.fontPx_);
            else if (tabOpts_.fontPx_ > 0)  btn = btn.font(tabOpts_.fontPx_);

            // 应用背景色选项(与输入顺序解耦,状态优先级:selected/unselected > Default)
            // 注意:Default 状态必须用 .bg() 合并,不能走 applyOptions ——
            // applyOptions 对 Default 是整体赋值 options,会清掉上面
            // 已设置的 size/fg/ttf/center 等属性。
            // 阶段1:Default 兜底(对全部按钮)
            for (const auto& opt : bgOptions_) {
                if (opt.p.t != tabview::part::first) continue;
                if (opt.st.value == elements::state::Default) {
                    btn = btn.bg(opt.color);
                }
            }
            // 阶段2:选中/未选中覆盖 Default(与条目顺序无关)
            for (const auto& opt : bgOptions_) {
                if (opt.p.t != tabview::part::first) continue;
                if (isSelected(opt.st) && i == active) {
                    btn = btn.bg(opt.color);
                } else if (isUnselected(opt.st) && i != active) {
                    btn = btn.bg(opt.color);
                }
            }
            // 阶段3:其余交互状态(pressed 等)写入 stateStyles,由后端按状态位覆盖
            for (const auto& opt : bgOptions_) {
                if (opt.p.t != tabview::part::first) continue;
                if (opt.st.value == elements::state::Default ||
                    isSelected(opt.st) || isUnselected(opt.st)) continue;
                btn.applyOptions(opt.st,
                    elements::attrOptions().bg(opt.color));
            }

            // 应用一级菜单各状态样式
            for (const auto& kv : firstStyles_) {
                btn.applyOptions(kv.first, kv.second);
            }
            menu.push_back(btn);
        }
        menu.push_back(skiff::Spacer());

        Element tabBar = skiff::VStack(menu, tabOpts_.itemSpacing_)
            .size(tabOpts_.menuWidth_, height)
            .pad(tabOpts_.menuPad_);

        // 右侧内容区
        int cur = active;
        Element content = items_[cur].content;
        content = content.key(std::to_string(cur).c_str());
        if (tabOpts_.slideIn_) content = content.slideInRight();

        // 应用内容区各状态样式
        for (const auto& kv : contentStyles_) {
            content.applyOptions(kv.first, kv.second);
        }

        Element contentWrap = skiff::VStack({content}, 0)
            .size(0, height)
            .expand();

        // 应用内容区背景色选项:Default 状态直接用 .bg() 合并,
        // 其他状态才写入 stateStyles,避免覆盖 size/expand 等已有选项
        for (const auto& opt : bgOptions_) {
            if (opt.p.t != tabview::part::content) continue;
            if (opt.st.value == elements::state::Default) {
                contentWrap = contentWrap.bg(opt.color);
            } else {
                contentWrap.applyOptions(opt.st,
                    elements::attrOptions().bg(opt.color));
            }
        }

        children = {tabBar, contentWrap};
    }
};

} // namespace components
} // namespace skiff
