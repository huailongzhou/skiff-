// TabView:左侧页签菜单 + 右侧内容区的组合组件(纯 DSL 组合,后端无关)。
// TabViewView 继承 ElementView,可通过 .build() 展开成通用 Element 树。
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
    bool slideIn_;

    TabViewOptions()
        : menuWidth_(200), menuPad_(10),
          itemWidth_(180), itemHeight_(48), itemSpacing_(8),
          itemFg_(0xFFFFFF), slideIn_(true) {}

    TabViewOptions& menuWidth(int w) { menuWidth_ = w; return *this; }
    TabViewOptions& menuPad(int p) { menuPad_ = p; return *this; }
    TabViewOptions& itemSize(int w, int h) {
        itemWidth_ = w; itemHeight_ = h; return *this;
    }
    TabViewOptions& itemSpacing(int s) { itemSpacing_ = s; return *this; }
    TabViewOptions& itemFg(uint32_t c) { itemFg_ = c; return *this; }
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

class TabViewView : public ElementView {
public:
    TabViewView(const std::vector<TabViewItem>& items, State<int>& tab)
        : items_(items), tab_(&tab) {
        e_->kind = Element::Row;
        e_->options.spacingPx = 0;
        e_->options.fontPx = 18;
    }

    // 对指定部分/状态批量应用样式(参考 LVGL 的 state selector)
    TabViewView& applyOptions(tabview::part p,
                              const elements::state& s,
                              const elements::attrOptions& opt) {
        if (p.t == tabview::part::first) {
            firstStyles_[s] = opt;
        } else {
            contentStyles_[s] = opt;
        }
        return *this;
    }

    // 批量设置背景色:{{part, state, color}, ...}
    TabViewView& applyBgOption(std::initializer_list<tabview::bgOption> opts) {
        bgOptions_.assign(opts);
        return *this;
    }

    TabViewView& itemSize(int w, int h) {
        tabOpts_.itemSize(w, h); return *this;
    }

    TabViewView& menuWidth(int w) {
        tabOpts_.menuWidth_ = w; return *this;
    }

    TabViewView& itemFg(uint32_t c) {
        tabOpts_.itemFg_ = c; return *this;
    }

    Element build() const override {
        if (items_.empty()) {
            Element empty;
            empty.kind = Element::Row;
            return empty;
        }
        int active = tab_->get();
        if (active < 0 || active >= (int)items_.size()) active = 0;
        // 指定了像素高度则用它;否则拉满父容器,不默认 432(=480-48)
        const bool hasPxH = e_->options.height > 0;
        const int height = hasPxH ? e_->options.height : 0;

        // 左侧页签栏
        std::vector<Element> menu;
        for (int i = 0; i < (int)items_.size(); ++i) {
            State<int>* tabPtr = tab_;
            int idx = i;
            ElementView btn = skiff::Button(items_[i].title,
                                            [tabPtr, idx] { tabPtr->set(idx); })
                .size(tabOpts_.itemWidth_, tabOpts_.itemHeight_)
                .fg(tabOpts_.itemFg_);
            if (!e_->options.ttfPath.empty()) btn.ttf(e_->options.ttfPath.c_str(), e_->options.fontPx);
            else if (e_->options.fontPx > 0)  btn.font(e_->options.fontPx);

            for (const auto& opt : bgOptions_) {
                if (opt.p.t != tabview::part::first) continue;
                if (opt.st.value == elements::state::Default) {
                    btn.bg(opt.color);
                }
            }
            for (const auto& opt : bgOptions_) {
                if (opt.p.t != tabview::part::first) continue;
                if (isSelected(opt.st) && i == active) {
                    btn.bg(opt.color);
                } else if (isUnselected(opt.st) && i != active) {
                    btn.bg(opt.color);
                }
            }
            for (const auto& opt : bgOptions_) {
                if (opt.p.t != tabview::part::first) continue;
                if (opt.st.value == elements::state::Default ||
                    isSelected(opt.st) || isUnselected(opt.st)) continue;
                btn.applyOptions(opt.st, elements::attrOptions().bg(opt.color));
            }

            for (const auto& kv : firstStyles_) {
                btn.applyOptions(kv.first, kv.second);
            }
            menu.push_back(btn.build());
        }
        menu.push_back(skiff::Spacer().build());

        ElementView tabBarView = skiff::VStack(menu, tabOpts_.itemSpacing_)
            .size(tabOpts_.menuWidth_, height)
            .pad(tabOpts_.menuPad_);
        if (!hasPxH) tabBarView.heightPct(100);
        Element tabBar = tabBarView.build();

        // 右侧内容区
        int cur = active;
        ElementView content(items_[cur].content);
        content.key(std::to_string(cur).c_str());
        if (tabOpts_.slideIn_) content.slideInRight();

        for (const auto& kv : contentStyles_) {
            content.applyOptions(kv.first, kv.second);
        }

        ElementView contentWrap = skiff::VStack({content.build()}, 0)
            .expand();
        if (hasPxH) contentWrap.size(0, height);
        else contentWrap.heightPct(100);

        for (const auto& opt : bgOptions_) {
            if (opt.p.t != tabview::part::content) continue;
            if (opt.st.value == elements::state::Default) {
                contentWrap.bg(opt.color);
            } else {
                contentWrap.applyOptions(opt.st, elements::attrOptions().bg(opt.color));
            }
        }

        Element root;
        root.kind = Element::Row;
        root.options = e_->options;
        root.children.push_back(tabBar);
        root.children.push_back(contentWrap.build());
        return root;
    }

private:
    std::vector<TabViewItem> items_;
    State<int>* tab_;
    TabViewOptions tabOpts_;

    std::vector<tabview::bgOption> bgOptions_;
    std::map<elements::state, elements::attrOptions> firstStyles_;
    std::map<elements::state, elements::attrOptions> contentStyles_;

    static bool isSelected(const elements::state& s) {
        return s.value == elements::state::Selected;
    }
    static bool isUnselected(const elements::state& s) {
        return s.value == elements::state::Unselected;
    }
};

// 兼容旧写法的工厂函数
inline TabViewView TabView(const std::vector<TabViewItem>& items, State<int>& tab) {
    return TabViewView(items, tab);
}

} // namespace components
} // namespace skiff
